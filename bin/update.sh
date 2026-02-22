#!/bin/bash

VENV_BIN="/var/www/mtgwebapp/venv/bin"

echo "Staging changes..."
git add /var/www/mtgwebapp/

# Requires that bump-my-version is installed and .bumpversion.cfg exists

OPTION="invalid"

while [ "$OPTION" == "invalid" ]; do
    echo "What kind of update is this?"
    echo "1) Patch (Bug fix)"
    echo "2) Minor (New feature)"
    echo "3) Major (Breaking change)"
    read -p "Selection [1-3]: " choice

    case $choice in
        1) OPTION="patch" ;;
        2) OPTION="minor" ;;
        3) OPTION="major" ;;
        *) OPTION="invalid"; echo -e "\n[!] Invalid selection. Please try again.\n" ;;
    esac
done

echo ""
read -p "Enter commit description: " commitvar

while [ -z "$commitvar" ]; do
    echo "[!] Commit message cannot be empty!"
    read -p "Enter commit description: " commitvar
done

echo "---------------------------------------"
if [ "$OPTION" == "patch" ]; then
    echo "Patching and comitting..."
else
    echo "$OPTION update and committing..."
fi



$VENV_BIN/bump-my-version bump $OPTION \
    --allow-dirty \
    --commit --tag --message "Bump version: {current_version} → {new_version} - $commitvar"

if [ $? -eq 0 ]; then
    echo "---------------------------------------"
    echo "Success! Version updated and tagged."
else
    echo "---------------------------------------"
    echo "Error: Something went wrong with the version bump."
fi

read -p "Push to origin? (y/n): " pushchoice
if [ "$pushchoice" == "y" ]; then
    git push origin main --tags
else
    echo "Ensure code is pushed to origin at a later time."
fi