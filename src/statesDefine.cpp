#include "statesDefine.hpp"

// ============= START STATE =============

struct {
    int readyCount = 0;
} internalReadyContext;

void UpdatePlayerReady(flecs::iter& iter, playerComp_t* players, color_t* colors) {
    for (auto i : iter) {
        if (players[i].IsReadyPressed() || players[i].IsReady()) {
            if(!players[i].IsReady()) // avoid making player marked 'dirty'
                players[i].SetIsReady(true);

            colors[i].SetColor(sf::Color::Green);
            internalReadyContext.readyCount++;
        }
    }
}

void IsAllPlayersReady(flecs::iter& iter) {
    if (internalReadyContext.readyCount == (int)iter.world().count<playerComp_t>()) {
        internalReadyContext.readyCount = 0;
        game->TransitionState(play);
    } else {
        internalReadyContext.readyCount = 0;
    }
}

struct {
    float curAngle = 0.0f;
} internalOrientcontext;

float PI = 3.14159265359f;

void OrientPlayers(flecs::iter& iter, transform_t* transforms) {
    sf::Vector2f middle = (sf::Vector2f)game->GetWindow().getSize() / 2.0f;

    float radii = 50.0f;
    float anglePerTurn = 2.0f * PI / iter.world().count<playerComp_t>();
    
    internalOrientcontext.curAngle += iter.delta_time();

    for(int i = 0; i < iter.count(); i++, internalOrientcontext.curAngle += anglePerTurn) {        
        sf::Vector2f location = {
            fastCos(internalOrientcontext.curAngle),
            fastSin(internalOrientcontext.curAngle),
        };

        location = location * radii + middle;
    
        transforms[i].SetPos(location);
        transforms[i].SetRot((location - middle).angle().asRadians());
    }
}

// ============= PLAY STATE =============

void UpdatePlayerDead(flecs::iter& iter, health_t* health) {
    play_t& playState = dynamic_cast<play_t&>(game->GetState());

    for(auto i : iter) {
        if(health[i].IsDestroyed()) {
            playState.AddDeadCount();            
        }
    }
}

void IsAllPlayersDead(flecs::iter& iter) {
    play_t& playState = dynamic_cast<play_t&>(game->GetState());

    if(playState.GetDeadCount() == iter.world().count<playerComp_t>()) {
        playState.ResetDeadCount();
        game->TransitionState(gameOver);
    } else {
        playState.ResetDeadCount();
    }
}

void IsDead(flecs::iter& iter, health_t* healths) {
    for (auto i : iter) {
        if (healths[i].GetHealth() <= 0.0f) {
            healths[i].SetDestroyed(true);
        }
    }
}

void PlayerPlayInputUpdate(flecs::iter& iter, playerComp_t* players, integratable_t* integratables, transform_t* transforms, health_t* healths) {
    float deltaTime = iter.delta_time();

    for (auto i : iter) {
        playerComp_t& player = players[i];
        integratable_t& integratable = integratables[i];
        transform_t& transform = transforms[i];

        player.AddTimer(deltaTime, healths[i].IsDestroyed());

        sf::Vector2f backwards = (transform.GetPos() - player.GetMouse()).normalized();
        sf::Vector2f left = backwards.perpendicular();

        if (player.GetMouse() != sf::Vector2f())
            transform.SetRot(backwards.angle().asRadians());

        if (player.IsUpPressed()) {
            integratable.AddLinearVelocity(-backwards * playerSpeed);
        }
        if (player.IsDownPressed()) {
            integratable.AddLinearVelocity(backwards * playerSpeed);
        }
        if (player.IsLeftPressed()) {
            integratable.AddLinearVelocity(left * playerSpeed * 0.5f);
        }
        if (player.IsRightPressed()) {
            integratable.AddLinearVelocity(-left * playerSpeed * 0.5f);
        }

        if (player.IsFirePressed() && player.GetLastFired() > playerFireRate) {
            player.ResetLastFired();
            player.SetIsFiring(true);

            if (iter.world().has<isHost_t>())
                iter.world().entity().set(
                    [&](transform_t& ctransform, integratable_t& integratable, color_t& color, shapeComp_t& shape) {
                        physicsWorld_t& world = *iter.world().get<physicsWorldComp_t>()->physicsWorld;

                        ctransform = transform;
                        color.SetColor(sf::Color::Yellow);

                        sf::Vector2f velocityDir = (player.GetMouse() - transform.GetPos()).normalized() * playerBulletSpeedMultiplier;
                        integratable.AddLinearVelocity(velocityDir);
                        integratables[i].AddLinearVelocity(-velocityDir * playerBulletRecoilMultiplier);

                        shape.shape = world.CreateShape<circle_t>(5.0f);
                    }).add<bulletComp_t>().add<isNetworked_t>();
        }
        else {
            player.SetIsFiring(false);
        }
    }
}

void PlayerBlinkUpdate(flecs::iter& iter, playerComp_t* players, health_t* healths, color_t* colors) {
    for (auto i : iter) {
        playerComp_t& player = players[i];
        health_t& health = healths[i];
        color_t& color = colors[i];

        if (player.GetLastBlink() <= 0.0f && health.IsDestroyed()) {
            color.SetColor(sf::Color::Blue);

            if(player.GetLastBlink() <= -0.5f) {
                player.ResetLastBlink();
            }
        }
        else if (color.GetColor() != sf::Color::Green) {
            color.SetColor(sf::Color::Green);
        }
    }
}

void PlayerReviveUpdate(flecs::iter& iter, sharedLives_t* lives, playerComp_t* players, health_t* healths) {
    for (auto i : iter) {
        playerComp_t& player = players[i];
        health_t& health = healths[i];

        if (health.IsDestroyed()) {
            if(lives->lives > 0) {
                if (player.GetTimeTillRevive() <= 0.0f) {
                    player.ResetTimeTillRevive();
                    health.SetDestroyed(false);
                    health.SetHealth(1.0f);

                    printf("Lost a life: %u\n", lives->lives);
                    lives->lives--;
                    lives->dirty = true;
                }
            } else {
                game->DisableEntity(iter.entity(i));
            }
        } 
    }
}

void AsteroidDestroyUpdate(flecs::iter& iter, asteroidComp_t*, health_t* healths) {
    for (auto i : iter) {
        health_t& health = healths[i];

        if (health.IsDestroyed()) {
            iter.entity(i).add<deferDelete_t>();
        }
    }
}

void TransformWrap(flecs::iter& iter, mapSize_t* size, transform_t* transforms) {
    for (auto i : iter) {
        transform_t& transform = transforms[i];
        sf::Vector2f pos = transform.GetPos();

        if (pos.x >= size->GetWidth()) {
            pos.x = 0.0f;
        }
        else if (pos.x <= 0.0f) {
            pos.x = size->GetWidth();
        }

        if (pos.y >= size->GetHeight()) {
            pos.y = 0.0f;
        }
        else if (pos.y <= 0.0f) {
            pos.y = size->GetHeight();
        }

        if (pos != transform.GetPos())
            transform.SetPos(pos);
    }
}

void AsteroidAddUpdate(flecs::iter& iter, physicsWorldComp_t* physicsWorld, mapSize_t* mapSize, asteroidTimer_t* timer) {
    timer->current -= iter.delta_time();
    if (timer->current < 0.0f) {
        timer->current = timer->resetTime;
        timer->resetTime -= timeToRemovePerAsteroidSpawn;

        float spawnX = (RandomFloat() * mapSize->GetWidth());
        float spawnY = (RandomFloat() * mapSize->GetHeight());

        float wallX = 0.0f;
        float distX = 0.0f;
        float distToLeft = spawnX;
        float distToRight = mapSize->GetWidth() - spawnX;
        if (distToLeft < distToRight) {
            wallX = mapSize->GetWidth() - 0.01f;
            distX = distToRight;
        }
        else {
            distX = distToLeft;
        }

        float wallY = 0.0f;
        float distY = 0.0f;
        float distToUp = spawnY;
        float distToDown = mapSize->GetHeight() - spawnY;
        if (distToUp < distToDown) {
            wallY = mapSize->GetHeight() - 0.1f;
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

        iter.world().entity()
            .add<isNetworked_t>()
            .add<asteroidComp_t>()
            .add<transform_t>()
            .add<health_t>()
            .add<integratable_t>()
            .add<color_t>()
            .add<asteroidComp_t>()
            .set([&](shapeComp_t& shape, transform_t& transform, integratable_t& integratable, color_t& color) {
            transform.SetPos({ spawnX, spawnY });

            sf::Vector2f center = mapSize->GetSize() / 2.0f;
            sf::Vector2f velToCenter = (transform.GetPos() - center).normalized();
            integratable.AddLinearVelocity(velToCenter * 10.0f);

            color.SetColor(sf::Color::Red);

            shape.shape = physicsWorld->physicsWorld->CreateShape<polygon_t>();
            polygon_t& polygon = physicsWorld->physicsWorld->GetPolygon(shape.shape);

            std::vector<sf::Vector2f> vertices = GenerateRandomConvexShape(8, 8.0f);
            polygon.SetVertices(vertices.size(), vertices.data());
            polygon.SetPos(transform.GetPos());
                });
    }
}

void ObservePlayerCollision(flecs::iter& iter, size_t i, shapeComp_t&) {
    flecs::entity entity = iter.entity(i);
    collisionEvent_t& event = *iter.param<collisionEvent_t>();
    flecs::entity other = event.entityOther;

    if (other.has<asteroidComp_t>()) {
        entity.set([](health_t& health) { health.SetHealth(0.0f); });
        game->PlayNoobSound();
    }
}

void ObserveBulletCollision(flecs::iter& iter, size_t i, shapeComp_t&) {
    flecs::entity entity = iter.entity(i);
    collisionEvent_t& event = *iter.param<collisionEvent_t>();
    flecs::entity other = event.entityOther;

    if (other.has<asteroidComp_t>()) {
        other.set([&](health_t& health) {
            health.SetHealth(health.GetHealth() - entity.get<bulletComp_t>()->damage);
        });

        game->PlayDestroySound();

        entity.add<deferDelete_t>();
    }
}