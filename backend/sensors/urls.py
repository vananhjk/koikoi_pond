from django.urls import path
from . import views

urlpatterns = [
    path('', views.index, name='dashboard'),
    path('api/upload/', views.api_upload_data, name='api_upload'),
]