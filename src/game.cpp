#include "game.hpp"

// ============= START STATE =============

struct {
    int readyCount = 0;
} internalReadyContext;

void updatePlayerReady(flecs::iter& iter, PlayerComponent* players, ColorComponent* colors, PlayerColorComponent* playerColors) {
    for (auto i : iter) {
        flecs::entity entity = iter.entity(i);
        
        if (players[i].isReadyPressed() || players[i].isReady()) {
            if(!players[i].isReady()) {// avoid making player marked 'dirty'
                players[i].setIsReady(true);
                entity.modified<PlayerComponent>();
            }

            colors[i].setColor(playerColors[i].getColor());
            internalReadyContext.readyCount++;

            entity.modified<ColorComponent>();
        }
    }
}

void isAllPlayersReady(flecs::iter& iter) {
    if (internalReadyContext.readyCount == (int)iter.world().count<PlayerComponent>()) {
        internalReadyContext.readyCount = 0;
        ae::transitionState<PlayState>();
    } else {
        internalReadyContext.readyCount = 0;
    }
}

struct {
    float curAngle = 0.0f;
} internalOrientContext;

float PI = 3.14159265359f;

void orientPlayers(flecs::iter& iter, ae::TransformComponent* transforms) {
    sf::Vector2f middle = (sf::Vector2f)ae::getWindow().getSize() / 2.0f;

    float radii = 50.0f;
    float anglePerTurn = 2.0f * PI / iter.world().count<PlayerComponent>();

    internalOrientContext.curAngle += iter.delta_time();

    for(int i = 0; i < iter.count(); i++, internalOrientContext.curAngle += anglePerTurn) {
        sf::Vector2f location = {
            ae::fastCos(internalOrientContext.curAngle),
            ae::fastSin(internalOrientContext.curAngle),
        };

        location = location * radii + middle;
    
        transforms[i].setPos(location);
        transforms[i].setRot((location - middle).angle().asRadians());

        iter.entity(i).modified<ae::TransformComponent>();
    }
}

// ============= PLAY STATE =============

inline struct InternalDeadCount {
    int deadCount = 0;
} internalDeadCount;

void updatePlayerDead(flecs::iter& iter, HealthComponent* health) {
    for(auto i : iter) {
        if(health[i].isDestroyed()) {
            internalDeadCount.deadCount++;
        }
    }
}

void isAllPlayersDead(flecs::iter& iter) {
    if(internalDeadCount.deadCount == iter.world().count<PlayerComponent>()) {
        internalDeadCount.deadCount = 0;
        ae::transitionState<GameOverState>();
    } else {
        internalDeadCount.deadCount = 0;
    }
}

void isDead(flecs::iter& iter, HealthComponent* healths) {
    for (auto i : iter) {
        flecs::entity entity = iter.entity(i);

        if (healths[i].getHealth() <= 0.0f) {
            healths[i].setDestroyed(true);
            entity.modified<HealthComponent>();
        }
    }
}

void playerPlayInputUpdate(flecs::iter& iter, PlayerComponent* players, ae::IntegratableComponent* integratables, ae::TransformComponent* transforms, HealthComponent* healths) {
    float deltaTime = iter.delta_time();
    ScoreComponent* score = iter.world().get_mut<ScoreComponent>();

    for (auto i : iter) {
        PlayerComponent& player = players[i];
        ae::IntegratableComponent& integratable = integratables[i];
        ae::TransformComponent& transform = transforms[i];

        player.addTimer(deltaTime, healths[i].isDestroyed());

        sf::Vector2f backwards = (transform.getPos() - player.getMouse()).normalized();
        sf::Vector2f left = backwards.perpendicular();

        if (player.getMouse() != sf::Vector2f())
            transform.setRot(backwards.angle().asRadians());

        if (player.isUpPressed()) {
            integratable.addLinearVelocity(-backwards * playerSpeed);
               iter.entity(i).modified<ae::IntegratableComponent>();
        }
        if (player.isDownPressed()) {
            integratable.addLinearVelocity(backwards * playerSpeed);
            iter.entity(i).modified<ae::IntegratableComponent>();
        }
        if (player.isLeftPressed()) {
            integratable.addLinearVelocity(left * playerSpeed * 0.5f);
            iter.entity(i).modified<ae::IntegratableComponent>();
        }
        if (player.isRightPressed()) {
            integratable.addLinearVelocity(-left * playerSpeed * 0.5f);
            iter.entity(i).modified<ae::IntegratableComponent>();
        }

         // make sure to only place turrets down and fire host side
        if(ae::getNetworkManager().hasNetworkInterface<ServerInterface>()) {
            if (player.isTurretPlacePressed() &&  // IS IT PLACE BUTTON PRESSED?
                player.getTurretPlaceCooldown() <= 0.0f &&  // IS THE PLACE COOLDOWN DOWN?
                iter.world().count<TurretComponent>() + 1 <= maxTurrets && // DOES PLACING ONE MORE SURPASS maxTurrets?
                (score->getScore() - turretPrice >= 0)) {

                score->removeScore(turretPrice);
                player.resetTurretPlaceCooldown();
                ae::getNetworkStateManager().entity()
                    .is_a<prefabs::Turret>()
                    .set([&](ae::TransformComponent& turretTranform) {
                        turretTranform.setPos(transform.getPos());
                    });

                iter.entity(i).modified<PlayerComponent>();
                iter.world().modified<ScoreComponent>();
            }

            if (player.isFirePressed() && player.getLastFired() > playerFireRate) {
                player.resetLastFired();
                player.setIsFiring(true);

                ae::getNetworkStateManager().entity()
                    .is_a<prefabs::Bullet>()
                    .set([&](ae::TransformComponent& bulletTransform, ae::IntegratableComponent& integratable, ae::ShapeComponent& shape) {
                        ae::PhysicsWorld& world = ae::getPhysicsWorld();

                        bulletTransform = transform;

                        sf::Vector2f velocityDir = (player.getMouse() - transform.getPos()).normalized() * playerBulletSpeed;
                        integratable.addLinearVelocity(velocityDir);
                        integratables[i].addLinearVelocity(-velocityDir * playerBulletRecoilMultiplier);

                        shape.shape = world.createShape<ae::Circle>(5.0f);
                    }).add<BulletComponent>();

                iter.entity(i).modified<PlayerComponent>();
            } else {
                player.setIsFiring(false);
            }
        }
    }
}

void playerBlinkUpdate(flecs::iter& iter, PlayerComponent* players, HealthComponent* healths, ColorComponent* colors, PlayerColorComponent* playerColors) {
    for (auto i : iter) {
        PlayerComponent& player = players[i];
        HealthComponent& health = healths[i];
        ColorComponent& color = colors[i];
        PlayerColorComponent& playerColor = playerColors[i];

        if (player.getLastBlink() <= 0.0f && health.isDestroyed()) {
            color.setColor(sf::Color::Blue);

            if(player.getLastBlink() <= -0.5f) {
                player.resetLastBlink();
            }

            iter.entity(i).modified<HealthComponent>();
        } else if (color.getColor() != playerColor.getColor()) {
            color.setColor(playerColor.getColor());
            iter.entity(i).modified<HealthComponent>();
        }
    }
}

void playerReviveUpdate(flecs::iter& iter, SharedLivesComponent* lives, PlayerComponent* players, HealthComponent* healths) {
    for (auto i : iter) {
        PlayerComponent& player = players[i];
        HealthComponent& health = healths[i];

        if (health.isDestroyed()) {
            if(lives->lives > 0) {
                if (player.getTimeTillRevive() <= 0.0f) {
                    player.resetTimeTillRevive();
                    health.setDestroyed(false);
                    health.setHealth(1.0f);

                    lives->lives--;
                    iter.world().modified<SharedLivesComponent>();
                    iter.entity(i).modified<HealthComponent>();
                }
            } else {
               ae::getNetworkStateManager().disable(iter.entity(i));
            }
        } 
    }
}

void createChildAsteroids(flecs::world world, ae::TransformComponent& parentTransform, ae::IntegratableComponent& parentIntegratable, u8 parentStage) {
    ae::PhysicsWorld& physicsWorld = ae::getPhysicsWorld();

    sf::Vector2f linearVelocity = parentIntegratable.getLinearVelocity() * asteroidDestroySpeedMultiplier;

    for(u32 i = 0; i < 2; i++) {
        ae::getNetworkStateManager().entity()
            .is_a<prefabs::Asteroid>()
            .set([&](AsteroidComponent& asteroid, 
                     ae::ShapeComponent& shape, 
                     ae::TransformComponent& transform, 
                     ae::IntegratableComponent& integratable) {
                transform.setPos(parentTransform.getPos());

                integratable.addLinearVelocity(linearVelocity);
                linearVelocity *= -1.0f;

                shape.shape = physicsWorld.createShape<ae::Polygon>();
                ae::Polygon& polygon = physicsWorld.getPolygon(shape.shape);

                asteroid.stage = parentStage - 1;

                std::vector<sf::Vector2f> vertices = generateRandomConvexShape(8, ((float)asteroid.stage / (float)initialAsteroidStage) * asteroidScalar);
                polygon.setVertices((u8)vertices.size(), vertices.data());
                polygon.setPos(transform.getPos());
            });
    }
}

void asteroidDestroyUpdate(flecs::iter& iter, AsteroidComponent* asteroids, ae::TransformComponent* transforms, ae::IntegratableComponent* integratables, HealthComponent* healths) {
    for (auto i : iter) {
        HealthComponent& health = healths[i];
        AsteroidComponent& asteroid = asteroids[i];

        if (health.isDestroyed()) {
            iter.entity(i).destruct();

            if(asteroid.stage > 1) {
                createChildAsteroids(iter.world(), transforms[i], integratables[i], asteroid.stage);
            }
        }
    }
}

sf::Vector2f wrap(MapSizeComponent* size, sf::Vector2f pos) {
    if (pos.x >= size->getWidth()) {
        pos.x = 0.0f;
    }
    else if (pos.x <= 0.0f) {
        pos.x = size->getWidth();
    }

    if (pos.y >= size->getHeight()) {
        pos.y = 0.0f;
    }
    else if (pos.y <= 0.0f) {
        pos.y = size->getHeight();
    }

    return pos;
}

void transformWrap(flecs::iter& iter, MapSizeComponent* size, ae::TransformComponent* transforms) {
    for (auto i : iter) {
        ae::TransformComponent& transform = transforms[i];
        sf::Vector2f pos = wrap(size, transform.getPos());

        if (pos != transform.getPos()) {
            transform.setPos(pos);
            iter.entity(i).modified<ae::TransformComponent>();   
        }
    }
}

void asteroidAddUpdate(flecs::iter& iter, MapSizeComponent* mapSize, AsteroidTimerComponent* timer) {
    ae::PhysicsWorld& physicsWorld = ae::getPhysicsWorld();

    timer->current -= iter.delta_time();
    if (timer->current < 0.0f) {
        timer->current = timer->resetTime;
        timer->resetTime -= timeToRemovePerAsteroidSpawn;

        float spawnX = (randomFloat() * mapSize->getWidth());
        float spawnY = (randomFloat() * mapSize->getHeight());

        float wallX = 0.0f;
        float distX = 0.0f;
        float distToLeft = spawnX;
        float distToRight = mapSize->getWidth() - spawnX;
        if (distToLeft < distToRight) {
            wallX = mapSize->getWidth() - 0.01f;
            distX = distToRight;
        }
        else {
            distX = distToLeft;
        }

        float wallY = 0.0f;
        float distY = 0.0f;
        float distToUp = spawnY;
        float distToDown = mapSize->getHeight() - spawnY;
        if (distToUp < distToDown) {
            wallY = mapSize->getHeight() - 0.1f;
            distY = distToDown;
        }
        else {
            distY = distToUp;
        }

        if (distX < distY) {
            spawnX = wallX;
        }
        else {
            spawnY = wallY;
        }

        ae::getNetworkStateManager().entity()
            .is_a<prefabs::Asteroid>()
            .set([&](ae::ShapeComponent& shape, ae::TransformComponent& transform, ae::IntegratableComponent& integratable) {
                transform.setPos({ spawnX, spawnY });

                sf::Vector2f center = mapSize->getSize() / 2.0f;
                sf::Vector2f velToCenter = (transform.getPos() - center).normalized();
                integratable.addLinearVelocity(velToCenter * 10.0f);

                shape.shape = physicsWorld.createShape<ae::Polygon>();
                ae::Polygon& polygon = physicsWorld.getPolygon(shape.shape);

                std::vector<sf::Vector2f> vertices = generateRandomConvexShape(8, asteroidScalar);
                polygon.setVertices((u8)vertices.size(), vertices.data());
                polygon.setPos(transform.getPos());
            });
    }
}

void turretPlayUpdate(flecs::iter& iter, ae::TransformComponent* transforms, TurretComponent* turrets) {
    ae::PhysicsWorld& physicsWorld = ae::getPhysicsWorld();
    flecs::world world = iter.world();
    
    std::vector<ae::SpatialIndexElement> results = {};
    for(auto i : iter) {
        flecs::entity entity = iter.entity(i);
        ae::TransformComponent& transform = transforms[i];
        TurretComponent& turret = turrets[i];

        turret.addTimer(iter.delta_time());
        transform.setRot(transform.getRot() + 0.1f);
        entity.modified<ae::TransformComponent>();

        ae::SpatialIndexTree& tree = physicsWorld.getTree();
        ae::AABB aabb(turretRange, turretRange, transform.getPos());

        results.clear();
        tree.query(spatial::intersects<2>(aabb.min.data(), aabb.max.data()), std::back_inserter(results));

        if(results.empty())
            continue;

        sf::Vector2f closestPos = {0.0f, 0.0f};
        float smallestDistance = std::numeric_limits<float>::max();
        for(ae::SpatialIndexElement& element : results) {
            flecs::entity other = ae::impl::af(element.entityId);
            if(!other.is_valid())
                continue;

            if(!other.has<AsteroidComponent>())
                continue;

            sf::Vector2f pos = physicsWorld.getShape(element.shapeId).getWeightedPos();
            float distance = (transform.getPos() - pos).length();
            if(distance < smallestDistance) {
                closestPos = pos;
                smallestDistance = distance;
            }
        }

        if(smallestDistance == std::numeric_limits<float>::max())
            continue;

        float angleToRotate = (closestPos - transform.getPos()).angle().asRadians();
        transform.setRot(angleToRotate);
    
        if(turret.getLastFired() <= 0.0f) {
            turret.resetLastFired();

            ae::getNetworkStateManager().entity()
                .is_a<prefabs::Bullet>()
                .set(
                [&](ae::TransformComponent& bulletTransform, 
                    ae::IntegratableComponent& integratable, 
                    ae::ShapeComponent& shape) {
                    ae::PhysicsWorld& world = ae::getPhysicsWorld();

                    bulletTransform = transform;

                    sf::Vector2f velocityDir = (closestPos - transform.getPos()).normalized() * playerBulletSpeed;
                    integratable.addLinearVelocity(velocityDir);

                    shape.shape = world.createShape<ae::Circle>(5.0f);
                }).add<BulletComponent>();
        }
    }
}

void observePlayerCollision(flecs::iter& iter, size_t i, ae::ShapeComponent&) {
    flecs::entity entity = iter.entity(i);
    ae::CollisionEvent& event = *iter.param<ae::CollisionEvent>();
    flecs::entity other = event.entityOther;

    if(entity.get<HealthComponent>()->isDestroyed())
        return;

    if (other.has<AsteroidComponent>()) {
        entity.set([](HealthComponent& health) { health.setHealth(0.0f); });
        global->getNoobPlayer.play();
    }
}

void observeBulletCollision(flecs::iter& iter, size_t i, ae::ShapeComponent&) {
    flecs::entity entity = iter.entity(i);
    ae::CollisionEvent& event = *iter.param<ae::CollisionEvent>();
    flecs::entity other = event.entityOther;

    if (other.has<AsteroidComponent>()) {
        other.set([&](HealthComponent& health) {
            health.setHealth(health.getHealth() - entity.get<BulletComponent>()->damage);
        });
        entity.destruct();
        global->destroyPlayer.play();
        iter.world().get_mut<ScoreComponent>()->addScore(scorePerAsteroid);
        iter.world().modified<ScoreComponent>();
    }
}