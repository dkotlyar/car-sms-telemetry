FROM python:3
ENV PYTHONUNBUFFERED=1
ENV DJANGO_SETTINGS_MODULE=beacon.settings
WORKDIR /code
COPY requirements.txt /code/
RUN pip install -r requirements.txt
COPY . /code/
RUN mkdir -p /code/beacon_main/migrations/ && \
    touch /code/beacon_main/migrations/__init__.py
CMD python manage.py runserver 0.0.0.0:8000