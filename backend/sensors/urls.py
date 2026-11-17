from django.urls import path
from . import views

urlpatterns = [
    path('', views.index, name='sensors_home'),  # Hoặc tên view hiển thị trang chính của bạn
]