#include "game.hpp"
#include "states.hpp"

player_t::player_t(gameWorld_t* world)
	: polygon(playerVertices), entity_t(world) {
	pos = {400.0f, 300.0f};
	polygon.SetPos(pos);
    SetColor(sf::Color::Green);
}

sf::Vector2f GetMouseWorldCoords() {
	sf::Vector2i mouseScreenCoords = sf::Mouse::getPosition(*game.window);

	//sf::View view = game.window->getView();
	game.debug.mouse = (sf::Vector2f)mouseScreenCoords;

	// Since we have not set the view, there is no transformation to be done.
	return (sf::Vector2f)mouseScreenCoords;
}

bool RandomBool() {
    return (bool)(rand() % 2);
}

std::vector<sf::Vector2f> GenerateRandomConvexShape(int size, float scale) {
    // Generate two lists of random X and Y coordinates
    std::vector<float> xPool;
    std::vector<float> yPool;

    for (int i = 0; i < size; i++) {
        xPool.push_back(RandomFloat() * 8.0f + 1.0f);
        yPool.push_back(RandomFloat() * 8.0f + 1.0f);
    }

    // Sort them
    std::sort(xPool.begin(), xPool.end());
    std::sort(yPool.begin(), yPool.end());

    // Isolate the extreme points
    float minX = xPool[0];
    float maxX = xPool[size - 1];
    float minY = yPool[0];
    float maxY = yPool[size - 1];

    // Divide the interior points into two chains & Extract the vector components
    std::vector<float> xVec;
    std::vector<float> yVec;

    float lastTop = minX, lastBot = minX;
    for (int i = 1; i < size - 1; i++) {
        float x = xPool[i];

        if (RandomBool()) {
            xVec.push_back(x - lastTop);
            lastTop = x;
        }
        else {
            xVec.push_back(lastBot - x);
            lastBot = x;
        }
    }

    xVec.push_back(maxX - lastTop);
    xVec.push_back(lastBot - maxX);

    float lastLeft = minY, lastRight = minY;
    for (int i = 1; i < size - 1; i++) {
        float y = yPool[i];

        if (RandomBool()) {
            yVec.push_back(y - lastLeft);
            lastLeft = y;
        }
        else {
            yVec.push_back(lastRight - y);
            lastRight = y;
        }
    }

    yVec.push_back(maxY - lastLeft);
    yVec.push_back(lastRight - maxY);

    // Randomly pair up the X- and Y-components
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(yVec.begin(), yVec.end(), g);

    // Combine the paired up components into vectors
    std::vector<sf::Vector2f> vec;

    for (int i = 0; i < size; i++) {
        vec.push_back({xVec[i], yVec[i]});
    }

    // Sort the vectors by angle
    std::sort(vec.begin(), vec.end(),
        [&](sf::Vector2f v1, sf::Vector2f v2) {
            return v1.angle() < v2.angle();
        });

    // Lay them end-to-end
    float x = 0, y = 0;
    float minPolygonX = 0;
    float minPolygonY = 0;
    std::vector<sf::Vector2f> points;

    for (int i = 0; i < size; i++) {
        points.push_back({x, y});

        x += vec[i].x;
        y += vec[i].y;

        minPolygonX = std::min(minPolygonX, x);
        minPolygonY = std::min(minPolygonY, y);
    }

    // Move the polygon to the original min and max coordinates
    float xShift = minX - minPolygonX;
    float yShift = minY - minPolygonY;

    for (int i = 0; i < size; i++) {
        sf::Vector2f p = points[i];
        points[i] = sf::Vector2f(p.x + xShift, p.y + yShift) * scale;
    }

    return points;
}

std::vector<sf::Vector2f> GetRandomPregeneratedConvexShape(float scale) {
    auto shape = *(asteroidHulls.begin() + (rand() % asteroidHulls.size()));

    for(int i = 0; i < shape.size(); i++) {
        shape[i].x *= scale;
        shape[i].y *= scale;
    }

    return shape;
}

void gameWorld_t::CheckCollision() {
    std::vector<int> bulletsToDestroy;
    std::vector<int> asteroidsToDestroy;

    for (u32_t asterI = 0; asterI < asteroids.size(); asterI++) {
        if (asteroids[asterI].IsDestroyed())
            continue;

        for (int i = 0; i < 2; i++) {
            if (players[i].IsDestroyed()) {
                continue;
            }

            collision_manifold_t manifold;
            if (TestCollision(asteroids[asterI].polygon, players[i].polygon, manifold)) {
                lives--;
                players[i].MarkDestroyed();
                game.getNoobPlayer.play();
            }
        }

        for (u32_t i = 0; i < bullets.size(); i++) {
            if (bullets[i].IsDestroyed())
                continue;

            collision_manifold_t manifold;
            if (TestCollision(asteroids[asterI].polygon, bullets[i].circle, manifold)) {
                bullets[i].MarkDestroyed();
                bulletsToDestroy.push_back(i);

                asteroids[asterI].health -= bullets[i].bulletDamage;
                if (asteroids[asterI].health <= 0.0f) {
                    asteroidsToDestroy.push_back(asterI);
                    asteroids[asterI].MarkDestroyed();
                    score += scorePerAsteroid;
                    game.destroyPlayer.play();
                    break;
                }
            }
        }
    }

    std::sort(bulletsToDestroy.begin(), bulletsToDestroy.end());
    std::sort(asteroidsToDestroy.begin(), asteroidsToDestroy.end());

    int reduceAmount = 0;
    for (int i : bulletsToDestroy) {
        bullets.erase(bullets.begin() + (i - reduceAmount));

        reduceAmount++;
    }

    reduceAmount = 0;
    for (int i : asteroidsToDestroy) {
        sf::Vector2f pos = asteroids[i].GetPos();
        sf::Vector2f vel = asteroids[i].vel * 1.5f;
        int stage = asteroids[i].GetStage();

        asteroids.erase(asteroids.begin() + (i - reduceAmount));
        reduceAmount++;

        if (stage > 3) {
            continue;
        }

        asteroids_t asteroid1(this);
        asteroid1.SetPos(pos);
        asteroid1.AddVelocity(vel);
        asteroid1.SetStage(stage + 1);
        asteroid1.GenerateShape();

        asteroids_t asteroid2(this);
        asteroid2.SetPos(pos);
        asteroid2.AddVelocity(-vel);
        asteroid2.SetStage(stage + 1);
        asteroid2.GenerateShape();

        asteroids.push_back(asteroid1);
        asteroids.push_back(asteroid2);
    }
}

void player_t::Tick(gameStateEnum_t state, float deltaTime) {
    polygon.SetPos(pos);
    polygon.SetRot(rot);
    
    switch (state) {
    case gameStateEnum_t::play:
        rot = (pos - mouse).angle().asRadians();

        if (input & inputFlagBits_t::UP) {
            vel.y -= playerSpeed;
        }
        if (input & inputFlagBits_t::DOWN) {
            vel.y += playerSpeed;
        }
        if (input & inputFlagBits_t::LEFT) {
            vel.x -= playerSpeed;
        }
        if (input & inputFlagBits_t::RIGHT) {
            vel.x += playerSpeed;
        }

        lastFired += deltaTime;
        if (input & inputFlagBits_t::FIRE && lastFired > playerFireRate) {
            lastFired = 0.0f;
            isFiring = true;
        }
        else {
            isFiring = false;
        }

        lastBlink -= deltaTime;
        if (lastBlink <= 0.0f && IsDestroyed()) {
            lastBlink = 0.2f;
            SetColor(sf::Color::Cyan);
        }
        else {
            SetColor(sf::Color::Green);
        }

        if (destroyed && world->GetTotalLives() != 0) {
            timeTillRevive -= deltaTime;
            if (timeTillRevive <= 0.0f) {
                timeTillRevive = 5.0f;
                destroyed = false;
            }
        }
        break;
    case gameStateEnum_t::start: case gameStateEnum_t::gameOver:
        if (input & inputFlagBits_t::READY) {
            ready = true;
        }

        if(ready) {
            SetColor(sf::Color::Green);
        } else {
            SetColor(sf::Color::Red);
        }
        break;
    }
}

void game_t::TransitionState(gameStateEnum_t state) {
	switch (state) {
	case gameStateEnum_t::connecting:
		TransitionState<connecting_t>();
		break;
	case gameStateEnum_t::connectionFailed:
		TransitionState<connectionFailed_t>();
		break;
	case gameStateEnum_t::connected:
		TransitionState<connected_t>();
		break;
	case gameStateEnum_t::start:
		TransitionState<start_t>();
		break;
	case gameStateEnum_t::play:
		TransitionState<play_t>();
		break;
	case gameStateEnum_t::gameOver:
		TransitionState<gameOver_t>();
		break;
	default:
		break;
	}
}

void gameWorld_t::Render(sf::RenderWindow& window) {
    for (u8_t i = 0; i < 2; i++) {
        RenderPolygon(window, players[i].color, players[i].polygon);
    }

    for (asteroids_t& asteroid : asteroids) {
        RenderPolygon(window, asteroid.color, asteroid.polygon);
    }

    for (bullet_t& bullet : bullets) {
        sf::CircleShape shape(bullet.circle.GetRadius());
        shape.move(bullet.circle.GetPos());
        shape.setFillColor(bullet.color);
        window.draw(shape);
    }

    sf::Text text(game.font);
    text.setString("Score: " + std::to_string(score) + "\nLives: " + std::to_string(lives));
    text.setStyle(sf::Text::Style::Bold | sf::Text::Style::Underlined);
    window.draw(text);
}