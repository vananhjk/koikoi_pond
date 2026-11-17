"""
URL configuration for backend project.

The `urlpatterns` list routes URLs to views. For more information please see:
    https://docs.djangoproject.com/en/6.1/topics/http/urls/
Examples:
Function views
    1. Add an import:  from my_app import views
    2. Add a URL to urlpatterns:  path('', views.home, name='home')
Class-based views
    1. Add an import:  from other_app.views import Home
    2. Add a URL to urlpatterns:  path('', Home.as_view(), name='home')
Including another URLconf
    1. Import the include() function: from django.urls import include, path
    2. Add a URL to urlpatterns:  path('blog/', include('blog.urls'))
"""
from django.contrib import admin
from django.http import HttpResponse
from django.urls import path, include

def home_view(request):
    return HttpResponse("<h1>Koi Pond Monitoring SaaS is Live!</h1><p>Hệ thống giám sát chất lượng nước hồ Koi đang hoạt động.</p>")

urlpatterns = [
    path('admin/', admin.site.urls),
    path('', home_view),
    path('', include('sensors.urls')),  # Đưa ra thư mục gốc để /login/, /logout/, / hoạt động chuẩn xác
]