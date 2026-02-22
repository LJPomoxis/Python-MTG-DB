import mylibs.DBhelperfunctions as DBHF
import os.path
from flask import Flask, request, redirect, render_template, g
from dotenv import load_dotenv
import MySQLdb
import random
import logging
import requests
import re

"""
DEV NOTE

If we want to create any actual user based web server, we will need to refactor many
of the MySQL interactions to remove any race conditions that could cause data collisions
to happen in the database
"""

app = Flask(__name__)

if __name__ != '__main__':
    gunicorn_logger = logging.getLogger('gunicorn.error')
    app.logger.handlers = gunicorn_logger.handlers
    app.logger.setLevel(gunicorn_logger.level)
    #app.logger.info("Flask app logger successfully configured under Gunicorn.")

load_dotenv()

MTG_APP_VERSION = "0.1.6"

EMAIL = os.getenv('EMAIL')
APP_INFO = F"mtgDB/{MTG_APP_VERSION} ({EMAIL})"

CUSTOM_HEADERS = {
    'User-Agent': APP_INFO,
    'Accept': 'application/json'
}

IMAGES_DIR_PATH = "/var/www/mtgwebapp/static/images/"
IMAGE_DISPLAY_PATH = "images/cards/"

NOT_DFC = ["normal", "meld", "class", "case", "mutate", "prototype", "saga"]

MANA_PATTERN = re.compile(r"\{([^}]+)\}")

app.config['DB_HOST'] = os.getenv('DB_HOST')
app.config['DB_USER'] = os.getenv('DB_USER')
app.config['DB_PASS'] = os.getenv('DB_PASS')
app.config['DB_NAME'] = os.getenv('DB_NAME')

def get_db():
    """
    Opens new db connection for the current context.
    """
    if 'db' not in g:
        g.db = MySQLdb.connect(
            host=app.config['DB_HOST'],
            user=app.config['DB_USER'],
            passwd=app.config['DB_PASS'],
            db=app.config['DB_NAME']
        )
    return g.db

def download_file(url, filename):
    filePath = IMAGES_DIR_PATH + filename
    try:
        response = requests.get(url, headers=CUSTOM_HEADERS, stream=True)
        response.raise_for_status()
        with open(filePath, 'wb') as file:
            for chunk in response.iter_content(chunk_size=8192):
                file.write(chunk)
    except requests.exceptions.RequestException as e:
        return False
    return True

def check_for_file(filename):
    filePath = IMAGES_DIR_PATH + filename
    return os.path.isfile(filePath)

# Returns list of html 
def clean_mana(manaVal):
    if not manaVal:
        return []

    symbols = MANA_PATTERN.findall(manaVal.lower())

    return [f'<span class="ms ms-{s.replace("/", "")}"></span>' for s in symbols]

@app.route('/')
@app.route('/index')
def home():
    conn = get_db()

    server = {}

    cursor = conn.cursor()
    cursor.execute("SELECT NOW()")
    server['serverTime'] = cursor.fetchone()[0]

    server['flavor'] = DBHF.get_random_flavor(cursor)

    # Gets 1 random card
    cursor.execute("""
        SELECT Ca.cardName, Ca.cardID, C.setID, Ci.bigImageUrl
        FROM CardAttributes Ca
        INNER JOIN Collection C
        ON Ca.cardID = C.cardID
        INNER JOIN CardImage Ci
        ON Ca.cardID = Ci.cardID
        AND C.setID = Ci.setID
        ORDER BY RAND()
        LIMIT 1             
    """)

    image = {}

    response = cursor.fetchone()
    server['cardName'] = response[0]
    server['ID'] = response[1]
    server['setID'] = response[2]
    image['url'] = response[3]

    filename = F"{server['ID']}-{server['setID']}.jpg"

    rand = random.randint(1,3)

    # 1 in 3 chance of downloading image if it isn't already, 
    # helps with load times and allows for local image hosting instead of from scryfall
    if check_for_file(filename):
        image['url'] = IMAGE_DISPLAY_PATH + filename
        image['local'] = True
    elif rand == 5:
        checkFile = download_file(image['url'], filename)
        if checkFile:
            image['url'] = IMAGE_DISPLAY_PATH + filename
            image['local'] = True
        else:
            image['local'] = False
    else:
        image['local'] = False

    server['image'] = image

    cursor.execute("""
        SELECT quantity
        FROM Collection
    """)

    response = cursor.fetchall()
    server['numberUnique'] = cursor.rowcount

    tmpTotal = 0
    for quantity in response:
        tmpTotal += quantity[0]

    server['collectionTotal'] = tmpTotal

    cursor.close()

    return render_template('index.html', server=server)

@app.route('/search', methods=['GET'])
def search():

    orderBy = 'cardName'

    if orderBy == 'cardID':
        orderBy = 'C.cardID'
    elif orderBy == 'cardName':
        orderBy = 'Ca.cardName'

    page = {}
    cardSearch = request.args.get('cardSearch')
    page['current'] = request.args.get('page', 1, type=int)
    
    perPage = 28
    offset = (page['current'] - 1) * perPage

    results = {}
    results['query'] = cardSearch

    if cardSearch:
        conn = get_db()
        cursor = conn.cursor()

        results['flavor'] = DBHF.get_random_flavor(cursor)
        formattedSearch = f'%{cardSearch}%'

        # Finish using triple quotes with %s to allow for string insertion
        query = """"""

        cursor.execute("""
            SELECT C.cardID, C.setID, C.quantity, Ca.cardName, Ci.bigImageUrl, COUNT(*) OVER() as totalCount
            FROM Collection C
            LEFT JOIN CardAttributes Ca ON C.cardID = Ca.cardID
            LEFT JOIN CardImage Ci ON C.cardID = Ci.cardID AND C.setID = Ci.setID
            WHERE Ca.cardName LIKE %s AND C.quantity > 0
            AND C.setID = (
                SELECT MAX(innerC.setID)
                FROM Collection innerC
                WHERE innerC.cardID = C.cardID       
                )
            ORDER BY Ca.cardName ASC
            LIMIT %s OFFSET %s
        """, (formattedSearch, perPage, offset))
        response = cursor.fetchall()

        if response:
            results['num'] = response[0][-1]
            page['total'] = (results['num'] + perPage - 1) // perPage
        else:
            page['total'] = 1
            results['num'] = 0
            cursor.close()
            return render_template('cardsearch.html', cards=None, results=results, page=page)
        
        cursor.close()

        cards = []

        # Not using is not None because fethall returns tuples which python counts as something
        # Instead we check if it is true, because python evaluates empty tuples as false
        if response:
            for cardID, setID, quantity, cardName, imageUrl, _ in response:
            
                filename = F"{cardID}-{setID}.jpg"
                local = False
                if check_for_file(filename):
                    imageUrl = IMAGE_DISPLAY_PATH + filename
                    local = True

                card = {
                    'ID': cardID,
                    'name': cardName,
                    'setID': setID,
                    'quantity': quantity,
                    'imageUrl': imageUrl,
                    'local': local
                }

                cards.append(card)
        else:
            cards = None

        return render_template('cardsearch.html', cards=cards, results=results, page=page)
    
    referrerUrl = request.referrer or '/'
    return redirect(referrerUrl)

@app.route('/card/<int:cardID>/<int:setID>')
def card_details(cardID, setID):
    conn = get_db()
    cursor = conn.cursor()

    # yikes, it works, but yikes
    cursor.execute("""
        SELECT Ca.cardName, C.quantity, Sl.setName, Sl.setCode, Ci.bigImageUrl, Co.oracle, Fl.flavor, Cmv.stringManaValue , Cpt.power, Cpt.toughness
        FROM Collection C
        LEFT JOIN CardAttributes Ca ON C.cardID = Ca.cardID
        LEFT JOIN SetLookup Sl ON C.setID = Sl.setID
        LEFT JOIN CardImage Ci ON C.cardID = Ci.cardID AND C.setID = Ci.setID
        LEFT JOIN CardOracle Co ON C.cardID = Co.cardID
        LEFT JOIN CardFlavor Cf ON C.cardID = Cf.cardID AND C.setID = Cf.setID
        LEFT JOIN FlavorLookup Fl ON Cf.flavorID = Fl.flavorID
        LEFT JOIN CardManaValue Cmv ON C.cardID = Cmv.cardID
        LEFT JOIN CardPT Cpt ON C.cardID = Cpt.cardID
        WHERE C.cardID = (%s) AND C.setID = (%s)
    """, (cardID, setID))
    response = cursor.fetchone()

    card = {}
    if response:
        cardName, quantity, setName, setCode, bigImageUrl, oracle, flavor, stringManaValue, power, toughness = response
        card = {
            'ID': cardID,
            'setID': setID,
            'name': cardName,
            'quantity': quantity,
            'setName': setName,
            'setCode': setCode,
            'image': bigImageUrl,
            'oracle': oracle,
            'flavor': flavor,
            'manaValue': stringManaValue,
            'power': power,
            'toughness': toughness
        }
    
    card['manaValue'] = clean_mana(card['manaValue'])

    cursor.execute("""
        SELECT Tl.type
        FROM CardType Ct
        INNER JOIN TypeLookup Tl
        ON Ct.cardTypeNumber = Tl.cardTypeNumber
        WHERE Ct.cardID = (%s)
    """, (cardID, ))
    response = cursor.fetchall()

    types = []
    for type in response:
        types.append(type[0])
    card['types'] = types

    cardList = []
    cursor.execute("""
            SELECT C.setID, Sl.setName, Ci.imageUrl
            FROM CardAttributes Ca
            INNER JOIN Collection C ON Ca.cardID = C.cardID
            INNER JOIN SetLookup Sl ON C.setID = Sl.setID
            INNER JOIN CardImage Ci ON C.cardID = Ci.cardID AND C.setID = Ci.setID
            WHERE C.cardID = (%s)
            AND C.setID <> (%s)
            ORDER BY C.setID
       """, (cardID, setID))
    response = cursor.fetchall()

    if response:
        for otherSetID, otherSetName, otherImage in response:
            otherCard = {
                'otherID': cardID,
                'otherSetID': otherSetID,
                'otherSetName': otherSetName,
                'otherImage': otherImage
            }

            cardList.append(otherCard)
    else:
        cardList = None

    cursor.close()

    filename = F"{cardID}-{setID}.jpg"
    if check_for_file(filename):
        card['image'] = IMAGE_DISPLAY_PATH + filename
        card['local'] = True
    else:
        card['local'] = False

    return render_template('carddetails.html', card=card, cardList=cardList)

@app.route('/editCollectionEntry', methods=['POST'])
def edit_collection_total():
    card = {}

    card['quantity'] = request.form.get('quantity', type=int)
    card['setID'] = request.form.get('setID')
    card['ID'] = request.form.get('cardID')

    conn = get_db()
    cursor = conn.cursor()
    collectionQuantity = DBHF.get_num_in_collection(card, cursor)

    referrerUrl = request.referrer or '/'
    if not collectionQuantity:
        redirect(referrerUrl)

    newVal = None
    if request.form.get('actions') == 'Add':
        newVal = int(collectionQuantity) + card['quantity']
    else:
        newVal = int(collectionQuantity) - card['quantity']
        if newVal < 0:
            newVal = 0

    query = F"UPDATE Collection SET quantity = {newVal} WHERE cardID = {card['ID']} AND setID = {card['setID']}"
    cursor.execute(query)
    conn.commit()

    cursor.close()
    return redirect(referrerUrl)

@app.route('/scryfalladdcard', methods=['POST', 'GET'])
def scryfall_query_card():
    conn = get_db()
    cursor = conn.cursor()
    setCodes = DBHF.get_all_setCodes(cursor)

    if request.method == 'POST':
        card = {}

        cardName = request.form.get('cardName')
        sanitizedCardName = re.sub(r'[^a-zA-Z0-9\s]', '', cardName)
        card['name'] = cardName
        card['cleanName'] = sanitizedCardName
        card['set'] = request.form.get('cardSetCode')
        card['quantity'] = request.form.get('cardQuantity')

        if not card['name'] or not card['set'] or not card['quantity']:
            return render_template('scryfallcardform.html', check=2, setCodes=setCodes, results="All fields must be filled")

        searchName = sanitizedCardName.replace(' ', '+')
        url = f"https://api.scryfall.com/cards/named?exact={searchName}&set={card['set']}"

        response = {}

        try:
            response = requests.get(url, headers=CUSTOM_HEADERS, timeout=(5, 10))
            response.raise_for_status() 
        except requests.exceptions.Timeout:
            return render_template('scryfallcardform.html', check=2, setCodes=setCodes, results="The Scryfall API timed out")
        except requests.exceptions.RequestException as e:
            return render_template('scryfallcardform.html', check=2, setCodes=setCodes, results=f"API Error: {e}")

        data = response.json()

        cards = []
        Dfc = data.get('layout')

        notDfc = Dfc in NOT_DFC

        if notDfc:
            card = DBHF.process_card_json(card, data, cursor)
            cards.append(card)
        else:
            cards = DBHF.process_dfc_json(card, data, cursor)

        for card in cards:
            if card['ID'] == 0:
                DBHF.create_new_card(card, cursor)
                conn.commit()
                card['ID'] = DBHF.get_cardID(card, cursor)

                card['colorID'] = DBHF.get_colorID(card['color'], cursor)
                card['colorIdentityID'] = DBHF.get_colorID(card['colorIdentity'], cursor)
                DBHF.add_cardColors(card, cursor)
                DBHF.add_cardOracle(card, cursor)
                DBHF.add_card_manaVal(card, cursor)
                DBHF.add_cardType(card, cursor)
                DBHF.add_cardKeyword(card, cursor)

                if card['power'] is not None:
                    DBHF.add_cardPT(card, cursor)

                if card.get('dfcID') is not None:
                    DBHF.add_dfcID(card, cursor)

                conn.commit()

            DBHF.add_to_collection(card, cursor)
            DBHF.add_cardFlavor(card, cursor)
            DBHF.add_cardImage(card, cursor)

        conn.commit()
        cursor.close()

        return render_template('scryfallcardform.html', check=1, setCodes=setCodes, results=cards)
    cursor.close()
    return render_template('scryfallcardform.html', check=0, setCodes=setCodes, results={})

@app.route('/deletecard', methods=['POST', 'GET'])
def delete_form():

    if request.method == 'POST':
        card = {}
        conn = get_db()
        cursor = conn.cursor()

        card['name'] = request.form.get('cardName')
        card['set'] = request.form.get('setCode')
        card['setID'] = DBHF.get_setID_from_setCode(card, cursor)
        if card['setID'] is None:
            referrerUrl = request.referrer or '/'
            return redirect(referrerUrl)
        card['ID'] = DBHF.get_cardID(card, cursor)
        if card['ID']:
            DBHF.delete_card(card, cursor)
            conn.commit()

        cursor.close()

        referrerUrl = request.referrer or '/'
        return redirect(referrerUrl)

    return render_template('deletecardform.html')

@app.route('/deckbuilder')
def deck_builder():
    decks = {}

    return render_template("deckbuilder.html", decks=decks)

@app.route('/deckadd', methods=['POST', 'GET'])
def new_deck():
    errors = None

    if request.method == 'POST':

        deckList = request.form.get('deckList')
        lines = deckList.splitlines()

        cards = []
        for line in lines:
            items = line.split()
            cards.append(items)

        errors=cards

        return render_template("newdeck.html", errors=errors)
    else:
        
        return render_template("newdeck.html", errors=errors)

@app.route('/deck/<int:deckID>')
def deck_details(deckID):

    deck = {'ID': deckID}

    return render_template('deckdetails.html', deck=deck)

@app.teardown_appcontext
def close_db(exception):
    conn = g.pop('db', None)

    if conn is not None:
        conn.close()

if __name__ == '__main__':
    app.run(debug=True)
