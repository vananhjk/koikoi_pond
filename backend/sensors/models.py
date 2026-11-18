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

    def is_warning(self):
        # Kiểm tra ngưỡng an toàn cho cá Koi: pH (6.5 - 8.5), Nhiệt độ (15°C - 30°C)
        if self.ph < 6.5 or self.ph > 8.5 or self.temperature < 15.0 or self.temperature > 30.0:
            return True
        return False

    def get_warning_message(self):
        if self.ph > 8.5:
            return f"Độ pH trong hồ đang quá cao (pH = {self.ph}), nguy cơ gây độc cho cá Koi!"
        elif self.ph < 6.5:
            return f"Độ pH trong hồ đang quá thấp (pH = {self.ph}), môi trường có tính axit!"
        elif self.temperature > 30.0:
            return f"Nhiệt độ nước quá cao ({self.temperature}°C), cần cấp thêm nước mát!"
        elif self.temperature < 15.0:
            return f"Nhiệt độ nước quá thấp ({self.temperature}°C), cá có thể giảm ăn!"
        return "Môi trường nước hồ Koi hoàn toàn tối ưu và an toàn."

    def __str__(self):
        return f"[{self.device_id}] {self.timestamp} - T: {self.temperature}°C - pH: {self.ph}"