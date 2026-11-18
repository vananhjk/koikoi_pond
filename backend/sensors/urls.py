from django.urls import path
from . import views

urlpatterns = [
    path('', views.index, name='sensors_home'),
    path('api/sensors/upload/', views.api_upload_data, name='api_upload'),
    path('api/sensors/data/', views.get_sensor_data_api, name='api_sensors_data'),
]