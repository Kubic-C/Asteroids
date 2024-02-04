#pragma once

#include "base.hpp"

class interactable_t {
public:
	virtual void OnHoverEnter() {}
	virtual void OnHoverExit() {}
	virtual void OnClick() {}
private:
};

class uiSpacer_t {
public:
	void SetMargin(sf::Vector2f margin) { this->margin = margin; }
	sf::Vector2f GetMargin() { return margin; }

	void SetMargin(sf::Vector2f padding) { this->padding = padding; }
	sf::Vector2f GetPadding() { return padding; }

private:
	sf::Vector2f margin;
	sf::Vector2f padding;
};

class button_t : public interactable_t, public sf::RectangleShape {
public:

	void Draw(sf::RenderWindow& window) {}
private:
	sf::Text* text;

	sf::Color buttonHoverColor;
	sf::Color textHoverColor;
	sf::Color textColor;
	sf::Color buttonColor;
};

class ui_t {
public:

private:

};