from django.shortcuts import render
from django.contrib.auth.decorators import login_required
from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
import json
from datetime import datetime
from .models import SensorData, UserDevice

# Hiển thị giao diện Dashboard (Yêu cầu đăng nhập)
@login_required(login_url='/accounts/login/')
def index(request):
    return render(request, 'sensors/dashboard.html')

# API nhận dữ liệu từ ESP32 (Không cần CSRF vì phần cứng gửi trực tiếp)
@csrf_exempt
def api_upload_data(request):
    if request.method == 'POST':
        try:
            data = json.loads(request.body)
            device_id = data.get('device_id', 'KOI_POND_01')
            temperature = data.get('temperature')
            turbidity = data.get('turbidity')
            tds = data.get('tds')
            ph = data.get('ph')

            # Lưu vào Database
            SensorData.objects.create(
                device_id=device_id,
                temperature=temperature,
                turbidity=turbidity,
                tds=tds,
                ph=ph
            )
            return JsonResponse({'status': 'success', 'message': 'Data saved successfully!'})
        except Exception as e:
            return JsonResponse({'status': 'error', 'message': str(e)}, status=400)
    return JsonResponse({'status': 'error', 'message': 'Only POST method allowed'}, status=405)

# API cung cấp dữ liệu JSON cho biểu đồ và thẻ Card trên giao diện
@login_required(login_url='/accounts/login/')
def get_sensor_data_api(request):
    try:
        # Lấy device_id liên kết với tài khoản đang đăng nhập
        user_device = request.user.device
        device_id = user_device.device_id
    except Exception:
        # Nếu tài khoản chưa gán thiết bị, lấy mặc định thiết bị đầu tiên hoặc rỗng
        device_id = 'KOI_POND_01'

    date_str = request.GET.get('date', None)
    queryset = SensorData.objects.filter(device_id=device_id).order_by('-timestamp')
    
    if date_str:
        try:
            filter_date = datetime.strptime(date_str, '%Y-%m-%d').date()
            queryset = queryset.filter(timestamp__date=filter_date)
        except ValueError:
            pass

    data_list = []
    for item in queryset[:100]: # Lấy 100 bản ghi mới nhất
        data_list.append({
            'temperature': item.temperature,
            'ph': item.ph,
            'turbidity': item.turbidity,
            'tds': item.tds,
            'timestamp': item.timestamp.strftime('%H:%M:%S %d/%m/%Y'),
            'warning': item.is_warning(),
            'message': item.get_warning_message()
        })
        
    return JsonResponse(data_list, safe=False)