from datetime import datetime, timedelta
import json
from django.shortcuts import render
from django.contrib.auth.decorators import login_required
from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
from django.utils import timezone
from .models import SensorData, UserDevice


@login_required(login_url='/accounts/login/')
def index(request):
    return render(request, 'sensors/dashboard.html')


@csrf_exempt
def api_upload_data(request):
    if request.method == 'POST':
        try:
            data = json.loads(request.body)
            device_id = data.get('device_id', 'KOI_POND_01')
            provided_key = request.headers.get('X-Device-Key')

            try:
                user_device = UserDevice.objects.get(device_id=device_id, is_active_sub=True)
                if user_device.api_key:
                    if not provided_key or provided_key != user_device.api_key:
                        return JsonResponse(
                            {'status': 'error', 'message': 'Invalid or missing X-Device-Key'},
                            status=403
                        )
            except UserDevice.DoesNotExist:
                pass

            SensorData.objects.create(
                device_id=device_id,
                temperature=data.get('temperature'),
                turbidity=data.get('turbidity'),
                tds=data.get('tds'),
                ph=data.get('ph')
            )
            return JsonResponse({'status': 'success', 'message': 'Data saved successfully!'})
        except Exception as e:
            return JsonResponse({'status': 'error', 'message': str(e)}, status=400)
    return JsonResponse({'status': 'error', 'message': 'Only POST method allowed'}, status=405)


def evaluate_four_params(item):
    t = item.temperature
    ph = item.ph
    turb = item.turbidity
    tds = item.tds

    if t < 15.0:
        temp_state = {'level': 'danger', 'label': 'Đỏ - Vượt ngưỡng', 'note': 'Nước quá lạnh'}
    elif t > 30.0:
        temp_state = {'level': 'danger', 'label': 'Đỏ - Vượt ngưỡng', 'note': 'Nước quá nóng'}
    elif (15.0 <= t < 20.0) or (28.0 < t <= 30.0):
        temp_state = {'level': 'warning', 'label': 'Vàng - Đáng báo động', 'note': 'Cận biên nhiệt độ (15-20°C hoặc 28-30°C)'}
    else:
        temp_state = {'level': 'normal', 'label': 'Xanh lá - An toàn', 'note': 'Nhiệt độ tối ưu (20-28°C)'}

    if ph < 6.0:
        ph_state = {'level': 'danger', 'label': 'Đỏ - Vượt ngưỡng', 'note': 'Nước bị axit'}
    elif ph > 8.0:
        ph_state = {'level': 'danger', 'label': 'Đỏ - Vượt ngưỡng', 'note': 'Nước bị kiềm'}
    elif (6.0 <= ph < 6.5) or (7.5 < ph <= 8.0):
        ph_state = {'level': 'warning', 'label': 'Vàng - Đáng báo động', 'note': 'Cận biên pH (6.0-6.5 hoặc 7.5-8.0)'}
    else:
        ph_state = {'level': 'normal', 'label': 'Xanh lá - An toàn', 'note': 'pH tối ưu (6.5-7.5)'}

    if turb > 50.0:
        turb_state = {'level': 'danger', 'label': 'Đỏ - Vượt ngưỡng', 'note': 'Cảnh báo chất lượng nước'}
    elif 25.0 <= turb <= 50.0:
        turb_state = {'level': 'warning', 'label': 'Vàng - Đáng báo động', 'note': 'Cảnh báo chất lượng nước (25-50 NTU)'}
    else:
        turb_state = {'level': 'normal', 'label': 'Xanh lá - An toàn', 'note': 'Độ đục an toàn (< 25 NTU)'}

    if tds > 500.0:
        tds_state = {'level': 'danger', 'label': 'Đỏ - Vượt ngưỡng', 'note': 'Cảnh báo chất lượng nước và độ dẫn điện'}
    elif (tds < 100.0) or (400.0 < tds <= 500.0):
        tds_state = {'level': 'warning', 'label': 'Vàng - Đáng báo động', 'note': 'Cảnh báo chất lượng nước và độ dẫn điện (<100 hoặc 400-500 ppm)'}
    else:
        tds_state = {'level': 'normal', 'label': 'Xanh lá - An toàn', 'note': 'TDS an toàn (100-400 ppm)'}

    return {
        'temperature': temp_state,
        'ph': ph_state,
        'turbidity': turb_state,
        'tds': tds_state,
    }


@login_required(login_url='/accounts/login/')
def get_sensor_data_api(request):
    try:
        device_id = request.user.device.device_id
    except Exception:
        device_id = 'KOI_POND_01'

    now_local = timezone.localtime(timezone.now())
    seven_days_ago_local = now_local - timedelta(days=7)

    queryset = SensorData.objects.filter(
        device_id=device_id,
        timestamp__gte=seven_days_ago_local
    ).order_by('-timestamp')

    date_str = request.GET.get('date')
    if date_str:
        try:
            filter_date = datetime.strptime(date_str, '%Y-%m-%d').date()
            if filter_date < seven_days_ago_local.date() or filter_date > now_local.date():
                return JsonResponse([], safe=False)
            queryset = [
                item for item in queryset
                if timezone.localtime(item.timestamp).date() == filter_date
            ]
        except ValueError:
            pass

    data_list = [
        {
            'temperature': item.temperature,
            'ph': item.ph,
            'turbidity': item.turbidity,
            'tds': item.tds,
            'timestamp': timezone.localtime(item.timestamp).strftime('%H:%M:%S %d/%m/%Y'),
            'warning': item.is_warning(),
            'message': item.get_warning_message(),
            'parameter_statuses': evaluate_four_params(item),
        }
        for item in list(queryset)[:200]
    ]
    return JsonResponse(data_list, safe=False)


def custom_404_view(request, exception):
    return render(request, 'sensors/404.html', status=404)


def custom_500_view(request):
    return render(request, 'sensors/500.html', status=500)