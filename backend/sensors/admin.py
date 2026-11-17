from django.contrib import admin
from .models import SensorData, UserDevice

@admin.register(SensorData)
class SensorDataAdmin(admin.ModelAdmin):
    list_display = ('id', 'device_id', 'temperature', 'turbidity', 'tds', 'ph', 'timestamp')

@admin.register(UserDevice)
class UserDeviceAdmin(admin.ModelAdmin):
    list_display = ('user', 'device_id', 'subscription_expires_at', 'is_active_sub')