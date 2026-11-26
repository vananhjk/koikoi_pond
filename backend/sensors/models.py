class SensorData(models.Model):
    device_id = models.CharField(max_length=100, db_index=True)
    temperature = models.FloatField()
    turbidity = models.FloatField()
    tds = models.FloatField()
    ph = models.FloatField()
    timestamp = models.DateTimeField(auto_now_add=True, db_index=True)

    # Ngưỡng tham khảo (chỉnh lại theo thông số thực tế của hồ Koi của bạn)
    PH_MIN, PH_MAX = 6.5, 8.5
    TEMP_MIN, TEMP_MAX = 15.0, 30.0
    TDS_MAX = 500.0
    TURBIDITY_MAX = 50.0

    def get_warnings(self):
        warnings = []
        if self.ph < self.PH_MIN:
            warnings.append(f"pH quá thấp ({self.ph})")
        elif self.ph > self.PH_MAX:
            warnings.append(f"pH quá cao ({self.ph})")

        if self.temperature < self.TEMP_MIN:
            warnings.append(f"Nhiệt độ quá thấp ({self.temperature}°C)")
        elif self.temperature > self.TEMP_MAX:
            warnings.append(f"Nhiệt độ quá cao ({self.temperature}°C)")

        if self.tds > self.TDS_MAX:
            warnings.append(f"TDS vượt ngưỡng ({self.tds} ppm)")

        if self.turbidity > self.TURBIDITY_MAX:
            warnings.append(f"Độ đục vượt ngưỡng ({self.turbidity} NTU)")

        return warnings

    def is_warning(self):
        return len(self.get_warnings()) > 0

    def get_warning_message(self):
        warnings = self.get_warnings()
        if not warnings:
            return "Môi trường nước hồ Koi hoàn toàn tối ưu và an toàn."
        return "Cảnh báo vượt ngưỡng: " + "; ".join(warnings) + "!"