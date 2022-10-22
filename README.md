# Инструкция по сборке и настройке проекта

## Web

Web-сервис состоит из двух компонент: 

1. Backend на базе Django (папка проекта: beacon-server)
2. Демонстрационная версия Frontend на базе Angular (папка проекта: beacon-web-demo)

### Развертывание Backend

1. Скопировать содержимое папки _beacon-server_ на сервер
2. Установить docker и docker-compose
3. Запустить файл build-docker.sh:
   ```
   $ bash build-docker.sh
   ```
4. Заменить содержимое файла docker-compose.yml файлом docker-compose-prod.yml:
   ```
   $ rm docker-compose.yml && mv docker-compose-prod.yml docker-compose.yml
   ```
4. Либо переместить файл docker-compose-prod.yml в любую другую папку из которой в последующем будет запускаться приложение
5. Запустить только сервис БД:
   ```
   $ docker-compose up -d postgres
   ```
6. Выполнить миграцию БД:
   ```
   $ docker-compose run backend python manage.py makemigrations
   $ docker-compose run backend python manage.py migrate
   ```
7. Создать супер-пользователя:
   ```
   $ docker-compose run backend python manage.py createsuperuser
   ```
8. Запустить приложение:
   ```
   $ docker-compose up -d backend
   ```

### Развертывание Frontend

1. Установить локально nodejs
2. Установить Angular:
   ```
   $ sudo npm i -g @angular/cli
   ```
3. Выполнить сборку проекта в папке _beacon-web-demo_:
   ```
   $ bash build.sh
   ```
4. Скопировать содержимое папки _docker_ на сервер
5. docker и docker-compose уже должны быть установлены на сервере
5. Запустить файл build-docker.sh
   ```
   $ bash build-docker.sh
   ```
6. Запустить frontend (в папке backend или папке куда был перемещен файл docker-compose-prod.yml):
   ```
   $ docker-compose up -d frontend
   ```

## Аппаратный уровень

Программное обеспечение для аппаратного уровня содержит программы для микрокомпьютера (используется NanoPi NEO) и микроконтроллера AT90CAN128

### Программирование микроконтроллера

Программное обеспечение находится в папке _firmware_.

1. Выполнить установку пакетов для сборки:
   ```
   $ sudo apt update && sudo apt install build-essential gcc-avr avr-libc avrdude 
   ```
2. Выполнить сборку приложения:
   ```
   $ make all
   ```
3. Выполнить прошивку микроконтроллера. В проекте используется JTAG программатор _AVR JTAG ICE Version 2.0_. Возможно использовать другой программатор, тогда необходимо в файле _Makefile_ заменить значение строчек _programmer_ и _port_ на соответствующие выбранному программатору в соответствии с инструкцией.
   ```
   $ make flash
   ```

### Настройка микрокомпьютера NanoPi NEO

1. Выполнить загрузку операционной системы [Armbian](https://www.armbian.com/nanopi-neo/) для NanoPI NEO
2. Установить и настроить в соответствии с [инструкцией](https://docs.armbian.com/User-Guide_Getting-Started/)
3. Установить docker и docker-compose для ARM процессоров

Данные для подключения к настроенному на данный момент компьютеру NanoPi NEO:

1. Подключиться по Ethernet или Wi-Fi (требуется USB-WiFi адаптер). 
2. Узнать IP адрес (для Wi-Fi должен быть 172.24.1.1)
3. Подключиться под пользователем _dkotlyar_
4. Пароль _nanopineo_

### Развертывание приложения для микрокомпьютера

1. Выполнить копирование содержимого папок _beacon-python_ и _local_django_ на микрокомпьютер
2. Выполнить компирование файла docker-compose.yml из корневой папки проекта на миркокомпьютер
3. В папке _beacon-python_ на микрокомпьютере выполнить сборку Docker-контейнера:
   ```
   $ docker build -t beacon/main .
   ```
4. В папке _local_django_ на микрокомпьютере выполнить сборку Docker-контейнера:
   ```
   $ bash build-docker.sh
   ```
5. В папке куда был скопирован файл docker-compose.yml выполнить первоначальный запуск веб-сервиса подобно запуску Backend на сервере:
   ```
   $ docker-compose up -d postgres
   $ docker-compose run backend python manage.py makemigrations
   $ docker-compose run backend python manage.py migrate
   $ docker-compose run backend python manage.py createsuperuser
   ```
6. Запустить приложение целиком:
   ```
   $ docker-compose up -d
   ```
