#include "game.hpp"
#include "statesDefine.hpp"

bool RandomBool() {
    return (bool)(rand() % 2);
}

std::vector<sf::Vector2f> generateRandomConvexShape(int size, float scale) {
    // Generate two lists of random X and Y coordinates
    std::vector<float> xPool;
    std::vector<float> yPool;

    for (int i = 0; i < size; i++) {
        xPool.push_back(randomFloat() * 8.0f + 1.0f);
        yPool.push_back(randomFloat() * 8.0f + 1.0f);
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

std::vector<sf::Vector2f> getRandomPregeneratedConvexShape(float scale) {
    std::vector<sf::Vector2f> shape = *(asteroidHulls.begin() + (rand() % asteroidHulls.size()));

    for(int i = 0; i < shape.size(); i++) {
        shape[i].x *= scale;
        shape[i].y *= scale;
    }

    return shape;
}
