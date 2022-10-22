from django.contrib import admin

from beacon_main.models import Telemetry, Car, Session, TelemetryRaw, MediaPart, Media

admin.site.register(Car)
admin.site.register(Session)
admin.site.register(TelemetryRaw)
admin.site.register(Telemetry)
admin.site.register(MediaPart)
admin.site.register(Media)
