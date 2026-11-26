from django.db import models
from django.contrib.auth.models import User


class UserDevice(models.Model):
    user = models.OneToOneField(User, on_delete=models.CASCADE, related_name='device')
    device_id = models.CharField(max_length=100, unique=True)
    is_active_sub = models.BooleanField(default=True)
    api_key = models.CharField(max_length=128, blank=True, null=True)
    subscription_expires_at = models.DateTimeField(blank=True, null=True)

    def __str__(self):
        return f"{self.device_id} ({self.user.username})"


class SensorData(models.Model):
    device_id = models.CharField(max_length=100, db_index=True)
    temperature = models.FloatField()
    turbidity = models.FloatField()
    tds = models.FloatField()
    ph = models.FloatField()
    timestamp = models.DateTimeField(auto_now_add=True, db_index=True)

    TEMP_SAFE_MIN, TEMP_SAFE_MAX = 20.0, 28.0
    TEMP_WARN_MIN, TEMP_WARN_MAX = 15.0, 30.0

    PH_SAFE_MIN, PH_SAFE_MAX = 6.5, 7.5
    PH_WARN_MIN, PH_WARN_MAX = 6.0, 8.0

    TURBIDITY_SAFE_MAX = 25.0
    TURBIDITY_WARN_MAX = 50.0

    TDS_SAFE_MIN, TDS_SAFE_MAX = 100.0, 400.0
    TDS_WARN_MAX = 500.0

    def get_warnings(self):
        warnings = []

        if self.temperature < self.TEMP_WARN_MIN:
            warnings.append(f"Nước quá lạnh ({self.temperature:.1f}°C)")
        elif self.temperature > self.TEMP_WARN_MAX:
            warnings.append(f"Nước quá nóng ({self.temperature:.1f}°C)")

        if self.ph < self.PH_WARN_MIN:
            warnings.append(f"Nước bị axit (pH {self.ph:.2f})")
        elif self.ph > self.PH_WARN_MAX:
            warnings.append(f"Nước bị kiềm (pH {self.ph:.2f})")

        if self.turbidity > self.TURBIDITY_WARN_MAX:
            warnings.append(f"Cảnh báo chất lượng nước - Độ đục cao ({self.turbidity:.1f} NTU)")

        if self.tds > self.TDS_WARN_MAX:
            warnings.append(f"Cảnh báo chất lượng nước và độ dẫn điện - TDS cao ({self.tds:.0f} ppm)")

        return warnings

    def is_warning(self):
        return len(self.get_warnings()) > 0

    def get_warning_message(self):
        warnings = self.get_warnings()
        if not warnings:
            return "Môi trường nước hồ Koi hoàn toàn tối ưu và an toàn."
        return "Cảnh báo vượt ngưỡng: " + "; ".join(warnings) + "!"