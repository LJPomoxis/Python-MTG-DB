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
of the MySQL interactions to remove any race conditions that could cause data fucky 
wuckys to happen in the database
"""

app = Flask(__name__)

if __name__ != '__main__':
    gunicorn_logger = logging.getLogger('gunicorn.error')
    app.logger.handlers = gunicorn_logger.handlers
    app.logger.setLevel(gunicorn_logger.level)
    #app.logger.info("Flask app logger successfully configured under Gunicorn.")

load_dotenv()

EMAIL = os.getenv('EMAIL')
APP_INFO = F"mtgDB/1.0 ({EMAIL})"

CUSTOM_HEADERS = {
    'User-Agent': APP_INFO,
    'Accept': 'application/json'
}

IMAGES_DIR_PATH = "/var/www/mtgwebapp/static/images/"
IMAGE_DISPLAY_PATH = "images/"

NOT_DFC = ["normal", "meld", "class", "case", "mutate", "prototype", "saga"]

COLORS_LIST = ["W", "U", "B", "R", "G"]

COLOR_MAP = {
    '00000': "Colorless",
    '00001': "Green",
    '00010': "Red",
    '00011': "Gruul",
    '00100': "Black",
    '00101': "Golgari",
    '00110': "Rakdos",
    '00111': "Jund",
    '01000': "Blue",
    '01001': "Simic",
    '01010': "Izzet",
    '01011': "Temur",
    '01100': "Dimir",
    '01101': "Sultai",
    '01110': "Grixis",
    '01111': "Glint",
    '10000': "White",
    '10001': "Selesnya",
    '10010': "Boros",
    '10011': "Naya",
    '10100': "Orhzov",
    '10101': "Abzan",
    '10110': "Mardu",
    '10111': "Dune",
    '11000': "Azorius",
    '11001': "Bant",
    '11010': "Jeskai",
    '11011': "Ink",
    '11100': "Esper",
    '11101': "Witch",
    '11110': "Yore",
    '11111': "Rainbow"
}

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

def convert_mana_cost_to_cmc(manaCost):
    #Converts mana cost string (e.g., "{2}{B}{B}{G}") into an integer 
    elements = re.findall(r"\{([^}]+)\}", manaCost)
    cmc = 0
    
    for element in elements:
        if element.isdigit():
            cmc += int(element)
        else:
            cmc += 1
            
    return cmc

def find_color_name(colors):
    if colors is None:
        colorKey = '00000'
    else:
        colorsTmp = [val in colors for val in COLORS_LIST]
        colorKey = "".join('1' if b else '0' for b in colorsTmp)
    
    return COLOR_MAP[colorKey]

def get_colorID(color, cursor):

    cursor.execute("""
        SELECT colorID
        FROM ColorLookup
        WHERE colorName = %s
    """, (color, ))

    response = cursor.fetchone()
    colorID = response[0]

    return colorID

def create_new_card(card, cursor):
    cursor.execute("""
        INSERT INTO CardAttributes (cardName, cleanCardName)
        VALUES (%s, %s)
    """, (card['name'], card['cleanName']))

def get_cardID(card, cursor):
    cursor.execute("""
        SELECT cardID
        FROM CardAttributes
        WHERE cardName = %s
    """, (card['name'], ))

    response = cursor.fetchone()

    if response is None:
        return 0
    else:
        cardID = response[0]

    #response = cursor.fetchall()
    #currentCardID = [field[0] for field in response]
    #cardID = curentCardID[0]

    return cardID

def get_setID_from_setCode(card, cursor):
    cursor.execute("""
        SELECT setID
        FROM SetLookup
        WHERE setCode = %s
    """, (card['set'], ))

    response = cursor.fetchone()
    setID = response[0]

    return setID

def get_random_flavor(cursor):
    cursor.execute("""
        SELECT Fl.flavor
        FROM CardFlavor Cf
        INNER JOIN FlavorLookup Fl
        ON Cf.flavorID = Fl.flavorID
        ORDER BY RAND()
        LIMIT 1       
    """)

    flavor = cursor.fetchone()[0]

    return flavor.replace('*',"<em>")

def get_num_in_collection(card, cursor):
    cursor.execute("""
        SELECT quantity FROM Collection
        WHERE cardID = %s AND setID = %s     
    """, (card['ID'], card['setID']))

    response = cursor.fetchone()
    if response is None:
        retVal = 0
    else:
        retVal = response[0]

    return retVal

#def add_new_card(): (maybe)

def add_to_collection(card, cursor):
    cursor.execute("""
        SELECT collectionID, quantity
        FROM Collection
        WHERE cardID = %s
        AND setID = %s
    """, (card['ID'], card['setID']))

    response = cursor.fetchone()

    if response is None:
        cursor.execute("""
            INSERT INTO Collection (cardID, setID, quantity)
            VALUES (%s, %s, %s)
        """, (card['ID'], card['setID'], card['quantity']))
    else:
        collectionID = response[0]
        quantityAdd = int(card['quantity']) + int(response[1])

        cursor.execute("""
            UPDATE Collection
            SET quantity = %s
            WHERE collectionID = %s
        """, (quantityAdd, collectionID))

def add_dfcID(card, cursor):
    cursor.execute("""
        INSERT INTO MultiFaceCards (cardID, dfcID)
        VALUES (%s, %s)
    """, (card['ID'], card['dfcID']))

def add_cardColors(card, cursor):
    cursor.execute("""
        INSERT INTO CardColors (cardID, colorID, colorIdentityID)
        VALUES (%s,%s,%s)
    """, (card['ID'], card['colorID'], card['colorIdentityID']))

def add_cardOracle(card, cursor):
    if card['oracle']:
        cursor.execute("""
            INSERT INTO CardOracle (cardID, oracle)
            VALUES (%s,%s)
        """, (card['ID'], card['oracle']))

def add_cardFlavor(card, cursor):
    if card['flavor']:

        flavorID = get_cardFlavorID(card['flavor'], cursor)
    
        cursor.execute("""
            INSERT IGNORE INTO CardFlavor (cardID, setID, flavorID)
            VALUES (%s,%s,%s)
        """, (card['ID'], card['setID'], flavorID))

def get_cardFlavorID(flavor, cursor):
    cursor.execute("""
        INSERT INTO FlavorLookup (flavor)
        VALUES (%s)
        ON DUPLICATE KEY UPDATE flavorID = LAST_INSERT_ID(flavorID)               
    """, (flavor, ))

    cursor.execute("SELECT LAST_INSERT_ID()")

    flavorID = cursor.fetchone()[0]
    return flavorID

def add_card_manaVal(card, cursor):
    cursor.execute("""
        INSERT INTO CardManaValue (cardID, manaValue, hasXinCost, stringManaValue)
        VALUES (%s,%s,%s,%s)
    """, (card['ID'], card['manaValue'], card['hasX'], card['stringManaValue']))

def add_cardPT(card, cursor):
    cursor.execute("""
        INSERT INTO CardPT (cardID, power, toughness)
        VALUES (%s, %s, %s)
    """, (card['ID'], card['power'], card['toughness']))

def add_cardImage(card, cursor):
    cursor.execute("""
        SELECT cardID
        FROM CardImage
        WHERE cardID = %s
        AND setID = %s
    """, (card['ID'], card['setID']))

    response = cursor.fetchone()

    if response is None:
        cursor.execute("""
            INSERT IGNORE INTO CardImage (cardID, setID, imageUrl, bigImageUrl)
            VALUES (%s, %s, %s, %s)
        """, (card['ID'], card['setID'], card['imageUrl'], card['bigImageUrl']))

def add_cardType(card, cursor):

    for type in card['types']:
        typeNumber = get_cardTypeNumber(type, cursor)
        cursor.execute("""
            INSERT INTO CardType (cardID, cardTypeNumber)
            VALUES (%s,%s)
        """, (card['ID'], typeNumber))

def get_cardTypeNumber(type, cursor):
    cursor.execute("""
        INSERT INTO TypeLookup (type)
        VALUES (%s)
        ON DUPLICATE KEY UPDATE cardTypeNumber = LAST_INSERT_ID(cardTypeNumber)
    """, (type, ))

    cursor.execute("SELECT LAST_INSERT_ID()")

    typeNumber = cursor.fetchone()[0]
    return typeNumber

def add_cardKeyword(card, cursor):
    for keyword in card['keywords']:
        keywordID = get_cardKeywordID(keyword, cursor)
        cursor.execute("""
            INSERT INTO CardKeywords (cardID, keywordID)
            VALUES (%s, %s)
        """, (card['ID'], keywordID))

def get_cardKeywordID(keyword, cursor):
    cursor.execute("""
        INSERT INTO KeywordLookup (keyword)
        VALUES (%s)
        ON DUPLICATE KEY UPDATE keywordID = LAST_INSERT_ID(keywordID)
    """, (keyword, ))

    cursor.execute("SELECT LAST_INSERT_ID()")

    keywordID = cursor.fetchone()[0]
    return keywordID

def get_all_setCodes(cursor):
    cursor.execute("""
        SELECT setCode
        FROM SetLookup
    """)

    response = cursor.fetchall()
    setCodes = [field[0] for field in response]

    return setCodes

def delete_card(card, cursor):

    cursor.execute("""
        DELETE FROM Collection C
        JOIN CardColors Cc ON C.cardID = Cc.cardID
        JOIN CardFlavor Cf ON C.cardID = Cf.cardID AND C.setID = Cf.setID
        JOIN CardManaValue Cmv ON C.cardID = Cmv.cardID
        JOIN CardPT Cpt ON C.cardID = Cpt.cardID
        JOIN CardOracle Ctx ON C.cardID = Ctx.cardID
        JOIN CardType Cty ON C.cardID = Cty.cardID
        JOIN CardImage Ci ON C.cardID = Ci.cardID
        JOIN CardKeywords Ck ON C.cardID = Ck.cardID
        JOIN MultiFaceCards Mfc ON C.cardID = Mfc.cardID
        WHERE C.cardID = %s AND C.setID = %s
    """, (card['ID'], card['setID']))

    #cursor.execute("""
    #    DELETE FROM CardAttributes
    #    WHERE CardID = %s
    #""", (card['ID'], ))

def process_dfc_json(baseCard, data, cursor):
    cards = []

    cardProperties = baseCard

    cursor.execute("""
        SELECT MAX(dfcID)
        FROM MultiFaceCards
    """)

    prevDfcID = cursor.fetchone()[0]
    dfcID = 0
    if prevDfcID is not None:
      dfcID = int(prevDfcID) + 1

    for face in data['card_faces']:

        card = cardProperties.copy()

        card['name'] = face['name'].replace(',', '')

        if face.get('colors'):
            card['color'] = find_color_name(face['colors'])
            card['colorIdentity'] = find_color_name(data['color_identity'])
        else:
            colors = [char for char in face.get('mana_cost') if char in COLORS_LIST]
            #app.logger.error(colors)
            card['color'] = find_color_name(colors)
            if data.get('color_identity') is None:
                card['colorIdentity'] = card['color']
            else:
                card['colorIdentity'] = find_color_name(data.get('color_identity'))

        card['manaValue'] = convert_mana_cost_to_cmc(face.get('mana_cost'))
        card['stringManaValue'] = face.get('mana_cost')

        card['keywords'] = data['keywords']
        cardTypes = face['type_line'].replace("—", "")
        card['types'] = cardTypes.split()
        card['oracle'] = face.get('oracle_text', "")
        card['flavor'] = face.get('flavor_text', "")
        
        card['power'] = face.get('power')
        if card['power'] == "*" or card['power'] == "X":
            card['power'] = 0
        card['toughness'] = face.get('toughness')
        if card['toughness'] == "*" or card['toughness'] == "X":
            card['toughness'] = 0

        card['hasX'] = face.get('mana_cost', '').find('X')

        uris = face.get('image_uris')
        if uris:
            card['imageUrl'] = uris.get('small')
            card['bigImageUrl'] = uris.get('normal')
        elif data['layout'] == "adventure":
            uris = data.get('image_uris')
            card['imageUrl'] = uris.get('small')
            card['bigImageUrl'] = uris.get('normal')
        else:
            card['imageUrl'] = data['image_uris']['small']
            card['bigImageUrl'] = data['image_uris']['normal']

        card['ID'] = get_cardID(card, cursor)
        card['setID'] = get_setID_from_setCode(card, cursor)
        card['dfcID'] = dfcID

        cards.append(card)

    return cards

def process_card_json(card, data, cursor):
    card['name'] = data['name']

    card['color'] = find_color_name(data['colors'])
    card['colorIdentity'] = find_color_name(data['color_identity'])

    card['manaValue'] = data['cmc']
    card['stringManaValue'] = data['mana_cost']
    card['keywords'] = data['keywords']
    cardTypes = data['type_line'].replace("—", "")
    card['types'] = cardTypes.split()
    card['oracle'] = data.get('oracle_text', "")
    card['flavor'] = data.get('flavor_text', "")

    card['power'] = data.get('power')
    card['toughness'] = data.get('toughness')
    if card['power'] == "*" or card['power'] == "X":
        card['power'] = 0
    if card['toughness'] == "*" or card['toughness'] == "X":
        card['toughness'] = 0

    card['hasX'] = data.get('mana_cost', '').find('X')
    card['imageUrl'] = data['image_uris']['small']
    card['bigImageUrl'] = data['image_uris']['normal']

    card['ID'] = get_cardID(card, cursor)
    card['setID'] = get_setID_from_setCode(card, cursor)

    return card

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

@app.route('/')
@app.route('/index')
def home():
    conn = get_db()

    server = {}

    cursor = conn.cursor()
    cursor.execute("SELECT NOW()")
    server['serverTime'] = cursor.fetchone()[0]

    server['flavor'] = get_random_flavor(cursor)

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

        results['flavor'] = get_random_flavor(cursor)
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
    collectionQuantity = get_num_in_collection(card, cursor)

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
    setCodes = get_all_setCodes(cursor)

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
            card = process_card_json(card, data, cursor)
            cards.append(card)
        else:
            cards = process_dfc_json(card, data, cursor)

        for card in cards:
            if card['ID'] == 0:
                create_new_card(card, cursor)
                conn.commit()
                card['ID'] = get_cardID(card, cursor)

                card['colorID'] = get_colorID(card['color'], cursor)
                card['colorIdentityID'] = get_colorID(card['colorIdentity'], cursor)
                add_cardColors(card, cursor)
                add_cardOracle(card, cursor)
                add_card_manaVal(card, cursor)
                add_cardType(card, cursor)
                add_cardKeyword(card, cursor)

                if card['power'] is not None:
                    add_cardPT(card, cursor)

                if card.get('dfcID') is not None:
                    add_dfcID(card, cursor)

                conn.commit()

            add_to_collection(card, cursor)
            add_cardFlavor(card, cursor)
            add_cardImage(card, cursor)

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
        card['setID'] = get_setID_from_setCode(card, cursor)
        if card['setID'] is None:
            referrerUrl = request.referrer or '/'
            return redirect(referrerUrl)
        card['ID'] = get_cardID(card, cursor)
        if card['ID']:
            delete_card(card, cursor)
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
