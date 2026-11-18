import json
from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
from django.shortcuts import render, redirect
from django.contrib.auth import authenticate, login, logout
from django.contrib.auth.decorators import login_required
from .models import SensorData, UserDevice
from datetime import datetime, timedelta
from django.utils import timezone
from django.shortcuts import render

def index(request):
    return render(request, 'dashboard.html')
@csrf_exempt
def receive_sensor_data(request):
    """API nhận dữ liệu từ ESP32"""
    if request.method == 'POST':
        try:
            data = json.loads(request.body)
            dev_id = data.get('device_id')
            temp = data.get('temperature')
            turbidity = data.get('turbidity')
            tds = data.get('tds')
            ph = data.get('ph')

            if not dev_id:
                return JsonResponse({'status': 'error', 'message': 'Missing device_id!'}, status=400)

            SensorData.objects.create(
                device_id=dev_id,
                temperature=temp,
                turbidity=turbidity,
                tds=tds,
                ph=ph
            )
            return JsonResponse({'status': 'success', 'message': 'Saved!'}, status=201)
        except Exception as e:
            return JsonResponse({'status': 'error', 'message': str(e)}, status=400)
    return JsonResponse({'status': 'error', 'message': 'Method not allowed'}, status=405)


@login_required(login_url='/login/')
def dashboard_view(request):
    """Trang giao diện Web User chính (Yêu cầu đăng nhập, trỏ về trang login tùy chỉnh)"""
    return render(request, 'sensors/dashboard.html')


@login_required(login_url='/login/')
def get_sensor_history(request):
    """API trả dữ liệu 4 thông số, phân quyền theo UserDevice, hỗ trợ lọc theo lịch"""
    try:
        user_device = request.user.device
        dev_id = user_device.device_id
    except UserDevice.DoesNotExist:
        return JsonResponse({'error': 'Tài khoản chưa được liên kết thiết bị Koi nào!'}, status=403)

    selected_date = request.GET.get('date') # Format: YYYY-MM-DD
    three_months_ago = timezone.now() - timedelta(days=90) # Giới hạn 3 tháng gần nhất
    
    queryset = SensorData.objects.filter(device_id=dev_id, timestamp__gte=three_months_ago)
    
    if selected_date:
        try:
            date_obj = datetime.strptime(selected_date, '%Y-%m-%d').date()
            queryset = queryset.filter(timestamp__date=date_obj)
        except ValueError:
            pass

    queryset = queryset.order_by('-timestamp')[:500]

    result = []
    for item in queryset:
        warning = False
        msg = "Môi trường nước hoàn hảo cho đàn Koi"
        
        # Logic cảnh báo chuẩn cá Koi
        if not (25 <= item.temperature <= 30):
            warning = True
            msg = "Cảnh báo: Nhiệt độ nước ngoài ngưỡng tối ưu (25-30°C)!"
        elif not (6.5 <= item.ph <= 8.5):
            warning = True
            msg = "Cảnh báo: Độ pH bất thường!"
        elif item.turbidity > 80:
            warning = True
            msg = "Cảnh báo: Nước quá đục (Độ đục > 80 NTU)!"

        result.append({
            'timestamp': item.timestamp.strftime('%Y-%m-%d %H:%M:%S'),
            'temperature': item.temperature,
            'turbidity': item.turbidity,
            'tds': item.tds,
            'ph': item.ph,
            'warning': warning,
            'message': msg
        })

    return JsonResponse(list(result), safe=False)


def user_login_view(request):
    """Trang đăng nhập dành riêng cho khách hàng SaaS Hồ Koi"""
    if request.user.is_authenticated:
        return redirect('dashboard')
        
    if request.method == 'POST':
        username = request.POST.get('username')
        password = request.POST.get('password')
        user = authenticate(request, username=username, password=password)
        if user is not None:
            login(request, user)
            return redirect('dashboard')
        else:
            return render(request, 'sensors/login.html', {'error': 'Sai tên đăng nhập hoặc mật khẩu!'})
    return render(request, 'sensors/login.html')


def user_logout_view(request):
    """Đăng xuất tài khoản và chuyển hướng thẳng về trang đăng nhập khách hàng"""
    logout(request)
    return redirect('user_login')