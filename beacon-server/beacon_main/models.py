from datetime import timedelta

from django.db import models

# Create your models here.


class DefaultModel(models.Model):
    create_datetime = models.DateTimeField(auto_now_add=True)
    update_datetime = models.DateTimeField(auto_now=True)


class Car(DefaultModel):
    imei = models.CharField(max_length=20, blank=True)

    @staticmethod
    def get_or_create(imei):
        if imei == '':
            return None

        car = Car.objects.filter(imei=imei)
        if len(car) > 0:
            return car[0]

        car = Car()
        car.imei = imei
        car.save()
        return car


class Session(DefaultModel):
    car = models.ForeignKey(Car, on_delete=models.PROTECT, null=True)
    session_datetime = models.DateTimeField(null=True)

    @staticmethod
    def get_or_create(car, dt):
        cond = {
            'car': car,
            'session_datetime__gt': dt - timedelta(seconds=3),
            'session_datetime__lt': dt + timedelta(seconds=3),
        }
        session = Session.objects.filter(**cond)
        if len(session) > 0:
            return session[0]
        session = Session()
        session.car = car
        session.session_datetime = dt


class TelemetryRaw(DefaultModel):
    imei = models.CharField(max_length=20, blank=True)
    ticks = models.IntegerField(default=0)
    obd2_timestamp = models.IntegerField(default=0)
    run_time = models.IntegerField(default=0)
    distance = models.IntegerField(default=0)
    engine_rpm = models.IntegerField(default=0)
    vehicle_kmh = models.IntegerField(default=0)
    gps_timestamp = models.IntegerField(default=0)
    gps = models.CharField(max_length=200)


class Telemetry(DefaultModel):
    session = models.ForeignKey(Session, on_delete=models.PROTECT)
    snapshot_datetime = models.DateTimeField(null=True)
    obd2_datetime = models.DateTimeField(null=True)
    gps_datetime = models.DateTimeField(null=True)
    run_time = models.IntegerField(default=0)
    distance = models.IntegerField(default=0)
    engine_rpm = models.IntegerField(default=0)
    vehicle_kmh = models.IntegerField(default=0)
    GNSS_run_status = models.BooleanField(default=False)
    fix_status = models.BooleanField(default=False)
    latitude = models.FloatField(null=True)
    longitude = models.FloatField(null=True)
    MLS_Altitude = models.FloatField(null=True)
    speed_over_ground = models.FloatField(null=True)
    course_over_ground = models.FloatField(null=True)
    fix_mode = models.IntegerField(null=True)
    HDOP = models.FloatField(null=True)
    PDOP = models.FloatField(null=True)
    VDOP = models.FloatField(null=True)
    satellites_in_view = models.IntegerField(null=True)
    satellites_used = models.IntegerField(null=True)
    cn0_max = models.IntegerField(null=True)
    HPA = models.FloatField(null=True)
    VPA = models.FloatField(null=True)

    class Meta:
        ordering = ['-gps_datetime']

    def __str__(self):
        return 'Telemetry #%d %s :: %f %f' % (self.id, self.imei, self.latitude or 0, self.longitude or 0)


class MediaPart(DefaultModel):
    car = models.ForeignKey(Car, on_delete=models.PROTECT, null=True)
    timestamp = models.BigIntegerField()
    format = models.CharField(max_length=10, blank=True)
    payload_filename = models.CharField(max_length=100)
    part = models.IntegerField()
    parts = models.IntegerField()


class Media(DefaultModel):
    car = models.ForeignKey(Car, on_delete=models.PROTECT, null=True)
    timestamp = models.BigIntegerField()
    format = models.CharField(max_length=10, blank=True)
    filepath = models.CharField(max_length=100)

