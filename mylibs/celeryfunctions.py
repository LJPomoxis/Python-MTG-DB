from celery import Celery
from time import time
from celery.exceptions import MaxRetriesExceededError
import requests
import DBhelperfunctions as DBHF

celery = Celery('MTGWebAppTasks',  backend='redis://localhost', broker='redis://localhost:6379/0')

celery.conf.task_routes = {
    'mylibs.tasks.fetch_card_data': {'queue': 'api_queue'},
}

@celery.task
def hello():
    return 'Hello World'

@celery.task(bind=True, max_retries=3)
def fetch_card_data(card_url, HEADER, NOT_DFC):

    # Cards passed here will already be checked to ensure that they aren't present in DB
    # The passed card also won't be a card but rather a prebuilt query
    # Thus any card passed here will be used as a query to scryfall
    # Use card details to add card to 'proxy' tables (These need to be designed and added to be used)

    # Handle failures like 404 or DNE for give card

    response = {}

    try:
        response = requests.get(card_url, headers=HEADER, timeout=(5, 10))
        response.raise_for_status() 
    except requests.exceptions.Timeout:
        return "API Timed Out"
    except requests.exceptions.RequestException as e:
        return f"API Error: {e}"

    data = response.json()
    time.sleep(1)

    # Celery will be stateless in this interaction
    # It will simply be handed data, query the api, process results, pass off results to DAL (Databse Access Layer)

    # DAL should function as a batcher

    pass

# We don't care if this fails, we check the file location each time we load an image
# so if the file isn't present then it is simply loaded from scryfall
@celery.task(ignore_result=True)
def background_file_download(url, filename, IMAGE_PATH, HEADER):
    filePath = IMAGE_PATH + filename
    try:
        response = requests.get(url, headers=HEADER, stream=True)
        response.raise_for_status()
        with open(filePath, 'wb') as file:
            for chunk in response.iter_content(chunk_size=8192):
                file.write(chunk)
    except requests.exceptions.RequestException as e:
        print(e)
    pass