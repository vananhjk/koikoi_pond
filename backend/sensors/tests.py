import json
from django.contrib.auth.models import User
from django.test import TransactionTestCase
from django.urls import reverse
from .models import SensorData, UserDevice


class Module3BackendTests(TransactionTestCase):
    def setUp(self):
        self.user = User.objects.create_user(username="koi_owner", password="secretpassword")
        self.valid_key = "koi-pond-01-valid-key"
        self.device = UserDevice.objects.create(
            user=self.user,
            device_id="KOI_POND_01",
            api_key=self.valid_key,
            is_active_sub=True,
        )
        self.upload_url = reverse("api_upload")
        self.valid_payload = {
            "device_id": "KOI_POND_01",
            "temperature": 24.5,
            "turbidity": 22.0,
            "tds": 240.0,
            "ph": 7.2,
        }

    # --- UT-M3-01 ---
    def test_ut_m3_01_1_valid_key_post_creates_sensor_data(self):
        before_count = SensorData.objects.count()
        response = self.client.post(
            self.upload_url,
            data=json.dumps(self.valid_payload),
            content_type="application/json",
            HTTP_X_DEVICE_KEY=self.valid_key,
        )
        self.assertEqual(response.status_code, 200)
        self.assertEqual(SensorData.objects.count(), before_count + 1)

    def test_ut_m3_01_2_missing_key_post_returns_403_without_saving(self):
        before_count = SensorData.objects.count()
        response = self.client.post(
            self.upload_url,
            data=json.dumps(self.valid_payload),
            content_type="application/json",
        )
        self.assertEqual(response.status_code, 403)
        self.assertEqual(SensorData.objects.count(), before_count)

    def test_ut_m3_01_3_wrong_key_post_returns_403_without_saving(self):
        before_count = SensorData.objects.count()
        response = self.client.post(
            self.upload_url,
            data=json.dumps(self.valid_payload),
            content_type="application/json",
            HTTP_X_DEVICE_KEY="wrong-device-key",
        )
        self.assertEqual(response.status_code, 403)
        self.assertEqual(SensorData.objects.count(), before_count)

    # --- UT-M3-02 ---
    def test_ut_m3_02_1_malformed_json_returns_400(self):
        before_count = SensorData.objects.count()
        response = self.client.post(
            self.upload_url,
            data="{bad_json",
            content_type="application/json",
            HTTP_X_DEVICE_KEY=self.valid_key,
        )
        self.assertEqual(response.status_code, 400)
        self.assertEqual(SensorData.objects.count(), before_count)

    def test_ut_m3_02_2_non_numeric_field_returns_400(self):
        before_count = SensorData.objects.count()
        bad_payload = dict(self.valid_payload, temperature="not_a_number")
        response = self.client.post(
            self.upload_url,
            data=json.dumps(bad_payload),
            content_type="application/json",
            HTTP_X_DEVICE_KEY=self.valid_key,
        )
        self.assertEqual(response.status_code, 400)
        self.assertEqual(SensorData.objects.count(), before_count)

    def test_ut_m3_02_3_missing_field_returns_400(self):
        before_count = SensorData.objects.count()
        incomplete_payload = {
            "device_id": "KOI_POND_01",
            "temperature": 24.5,
            "turbidity": 22.0,
            "tds": 240.0,
        }
        response = self.client.post(
            self.upload_url,
            data=json.dumps(incomplete_payload),
            content_type="application/json",
            HTTP_X_DEVICE_KEY=self.valid_key,
        )
        self.assertEqual(response.status_code, 400)
        self.assertEqual(SensorData.objects.count(), before_count)

    def test_warning_thresholds_on_sensor_data(self):
        safe_data = SensorData(
            device_id="KOI_POND_01",
            temperature=24.0,
            turbidity=20.0,
            tds=200.0,
            ph=7.0,
        )
        self.assertFalse(safe_data.is_warning())

        high_ph_data = SensorData(
            device_id="KOI_POND_01",
            temperature=24.0,
            turbidity=20.0,
            tds=200.0,
            ph=9.0,
        )
        self.assertTrue(high_ph_data.is_warning())
        self.assertIn("pH", high_ph_data.get_warning_message())