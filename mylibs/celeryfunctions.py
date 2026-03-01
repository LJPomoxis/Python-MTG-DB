from celery import Celery
from time import time
from celery.exceptions import MaxRetriesExceededError
import requests

celery = Celery('MTGWebAppTasks',  backend='redis://localhost', broker='redis://localhost:6379/0')

celery.conf.task_routes = {
    'MTGWebAppTasks.tasks.fetch_card_data': {'queue': 'api_queue'},
}

@celery.task
def hello():
    return 'Hello World'

@celery.task(bind=True, max_retries=3)
def fetch_card_data(card_data, APIHeader):
    # API logic here
    time.sleep(1)
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