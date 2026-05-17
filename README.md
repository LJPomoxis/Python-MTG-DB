# MTG Collection Manager
A custom, self-hosted web application for tracking your Magic: The Gathering card collection. Heavily inspired by Scryfall, and using the Scryfall API, this tool allows for easy card tracking and management.

## Features
- Easy Card Entry: With just an entry of the card name and set code, the backend queries the Scryfall API to get all of the card details.

- Searching: Searches based off of the card name with pagination allow for easy searching of your collection for the card you want.

- Card Views: With card art courtesy of Scryfall, the card view shows you the specifics of the card as well as.

- Inventory Management: Easily track and manage different printings of the same card and their quantities with the simple UI.

- Collection Widgets: The homepage features various data insights into the cards in your collection.

- Fast database writes managed through C++ application

## Tech Stack
- Backend: Python 3 (Flask)

- WSGI Server: Gunicorn

- DAL: C++ DAL Application

- Database: MariaDB (MySQL)

- Frontend: HTML5, CSS, JavaScript

- API Integrations: Scryfall API

## Roadmap
[ ] Advanced Search: Multi-parameter filtering (Color, CMC, Rarity, Power/Toughness).

[ ] Deck Builder: Integrated deck construction that directly uses the collection to asses card availability. Will include the ability to add proxy cards if they do not exist within the collection allowing for better building and testing capabilities.

[ ] Better Deck Builder Formatting: Either refactor formatting to be in line with manabox formatting (Should have checked from the start realistically).

[ ] Playtest Feature: A playtesting environment for playtesting decks built in the web app.

[ ] Refactor Python Database Interaction: Python should be limited to select statements to imporove speed and responsiveness.
