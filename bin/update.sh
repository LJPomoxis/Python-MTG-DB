#!/bin/bash

VENV_BIN="/var/www/mtgwebapp/venv/bin"

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
GRAY='\033[1;30m'
YELLOW='\033[1;33m'
LBLUE='\033[1;34m'
LCYAN='\033[1;36m'
NC='\033[0m'

printf "Staging changes...\n"
git add /var/www/mtgwebapp/

# Requires that bump-my-version is installed and .bumpversion.cfg or pyproject.toml exists

OPTION="invalid"

while [ "$OPTION" == "invalid" ]; do
    printf "${LCYAN}Select Update Type${NC}\n"
    printf "1) ${GRAY}Patch${NC} (Bug fix)\n"
    printf "2) ${BLUE}Minor${NC} (New feature)\n"
    printf "3) ${LBLUE}Major${NC} (Breaking change)\n"
    printf "${LCYAN}Selection${NC} [${GRAY}1-3${NC}]: "
    read choice

    case $choice in
        1) OPTION="patch" ;;
        2) OPTION="minor" ;;
        3) OPTION="major" ;;
        *) OPTION="invalid"; printf "\n${YELLOW}[!] Invalid selection.${NC} Please try again.\n" ;;
    esac
done

printf "\n"
printf "${LCYAN}Enter commit description${NC}: "
read commitvar

while [ -z "$commitvar" ]; do
    printf "${YELLOW}[!] Commit message cannot be empty!${NC}\n"
    printf "${LCYAN}Enter commit description${NC}: "
    read commitvar
done

echo "---------------------------------------"
if [ "$OPTION" == "patch" ]; then
    printf "${GREEN}Performing patch and comitting...${NC}\n"
else
    printf "${GREEN}Performing $OPTION update and committing...${NC}\n"
fi

$VENV_BIN/bump-my-version bump $OPTION \
    --allow-dirty \
    --commit --tag --message "Bump version: {current_version} → {new_version} - $commitvar"

if [ $? -eq 0 ]; then
    printf "---------------------------------------\n"
    printf "${GREEN}Success! Version updated and tagged.${NC}\n"
else
    printf "---------------------------------------\n"
    printf "${RED}Error: Something went wrong with the version bump.${NC}\n"
fi

printf "Push to origin? (${GREEN}y${NC}/${RED}n${NC}): "
read pushvar
if [ "$pushvar" == "y" ]; then
    git push origin main --tags
else
    printf "${YELLOW}Ensure code is pushed to origin at a later time.${NC}\n"
fi