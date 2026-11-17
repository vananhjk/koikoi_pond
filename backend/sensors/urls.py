from django.urls import path
from .views import receive_sensor_data, get_sensor_history, dashboard_view, user_login_view, user_logout_view

urlpatterns = [
    path('', dashboard_view, name='dashboard'),
    path('login/', user_login_view, name='user_login'),
    path('logout/', user_logout_view, name='user_logout'),
    path('upload/', receive_sensor_data, name='sensor_upload'),
    path('data/', get_sensor_history, name='sensor_history'),
]