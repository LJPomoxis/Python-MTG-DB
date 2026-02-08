import re


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