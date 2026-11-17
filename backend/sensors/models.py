from django.db import models
from django.contrib.auth.models import User
from datetime import timedelta
from django.utils import timezone

class UserDevice(models.Model):
    user = models.OneToOneField(User, on_delete=models.CASCADE, related_name='device')
    device_id = models.CharField(max_length=100, unique=True, verbose_name="Mã thiết bị ESP32")
    subscription_expires_at = models.DateTimeField(verbose_name="Hạn sử dụng dịch vụ", null=True, blank=True)
    is_active_sub = models.BooleanField(default=True, verbose_name="Trạng thái kích hoạt")

    def __str__(self):
        return f"{self.user.username} - {self.device_id}"

class SensorData(models.Model):
    # Liên kết dữ liệu trực tiếp với Device ID thay vì user để ESP32 dễ gửi dữ liệu lên
    device_id = models.CharField(max_length=100, db_index=True)
    temperature = models.FloatField()
    turbidity = models.FloatField()
    tds = models.FloatField()
    ph = models.FloatField()
    timestamp = models.DateTimeField(auto_now_add=True, db_index=True)

    def __str__(self):
        return f"[{self.device_id}] {self.timestamp} - T: {self.temperature}°C"