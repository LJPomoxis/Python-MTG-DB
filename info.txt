# Must run below command to work as venv contains libraries needed
source venv/bin/activate

# Must run below command to open mariadb sql shell and add/drop/edit tables/databases
mariadb -u mtgdbuser -p

# ENSURE that you commit any delete, insert, or update queries
db.commit()
# db assuming cursor = db.cursor()

# or conn = get_db()
# cursor = conn.cursor()
conn.commit()

