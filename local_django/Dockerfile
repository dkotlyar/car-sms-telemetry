FROM python:3.7-buster
ENV PYTHONUNBUFFERED=1
ENV DJANGO_SETTINGS_MODULE=local_django.settings
WORKDIR /code
COPY requirements.txt /code/
RUN pip install -r requirements.txt
COPY . /code/
RUN mkdir -p /code/beacon_local/migrations/ && \
    touch /code/beacon_local/migrations/__init__.py
CMD python manage.py runserver 0.0.0.0:8000