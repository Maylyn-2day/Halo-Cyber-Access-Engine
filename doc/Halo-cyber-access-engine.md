

## ĐỒ ÁN MÔN HỌC
## MÔN: CƠ SỞ LẬP TRÌNH
## HALO – CYBER ACCESS ENGINE

Bối cảnh
Một công ty công nghệ lớn gặp sự cố an ninh mạng nghiêm trọng. Hệ thống ghi nhận hàng
triệu sự kiện truy cập từ người dùng, thiết bị và ứng dụng khác nhau. Đội ngũ an ninh không
thể nhanh chóng phân tích dữ liệu vì lượng log quá lớn và việc tìm kiếm quá chậm.
Công ty quyết định xây dựng Halo — một engine phân tích truy cập giúp tổ chức dữ liệu
hiệu quả, tìm kiếm nhanh và hỗ trợ phát hiện hành vi đáng ngờ.
Trong đồ án này, sinh viên sẽ xây dựng Halo – Cyber Access Engine.

Mục tiêu
Sinh viên cần xây dựng một chương trình C++ có khả năng:
 Tìm kiếm nhanh trên dữ liệu lớn
 Phân tích hoạt động truy cập
 Phát hiện hành vi bất thường


Thiết lập hệ thống
Trước khi đưa vào sử dụng, hệ thống sẽ trải qua giai đoạn on boarding. Trong giai đoạn này,
dữ liệu về hoạt động của tổ chức sẽ được thu thập và cung cấp cho hệ thống dưới dạng các
log file có định dạng CSV. File này có cấu trúc như sau:
- Dòng đầu tiên là dòng tiêu đề gồm: định danh của người dùng, thiết bị, ứng dụng, tài
nguyên, loại sự kiện ứng dụng đang thực hiện, vị trí địa lý nơi thiết bị được sử dụng và
thời điểm sự kiện xảy ra (dạng epouch time).
- Các dòng sau đó là các thông tin tương ứng với từng cột dữ liệu đã nêu ở trên.
Chi tiết các cột dữ liệu:
Id   Tên cột Diễn giải
## 1
user_id
Định danh người dùng
## 2
device_id
Định danh thiết bị mà người dùng sử dụng
## 3
app_id
Định danh ứng dụng đang hoạt động trên thiết bị
## 4
resource_id
Định danh tài nguyên đang được truy xuất
## 5
event_type
Loại sự kiện đang được ứng dụng thực hiện, bao gồm:
## LOGIN, LOGOUT, TOKEN_REFRESH, ACCESS,
FAILED_LOGIN, OPEN_APP, DOWNLOAD và ADMIN_ACTION
## 6
location
Vị trí địa lý nơi thiết bị đang được sử dụng, có thể là: US, VN,
## JP, KR, SG, CN, DE, FR, UK, AU, CA, IN, BR, RU, TH
## 7
timestamp
Thời điểm sự kiện xảy ra

Ví dụ:

user_id,device_id,app_id,resource_id,event_type,location,timestamp
## U007,D018,APP003,R025,DOWNLOAD,SG,1713225863
## U005,D025,APP018,R018,OPEN_APP,CN,1713108845


Đánh giá giữa kì
A. Mục tiêu
Xây dựng nền tảng lưu trữ và tìm kiếm dữ liệu hiệu quả.
B. Thời lượng
Sinh viên hoàn tất trong 2 tuần.
C. Yêu cầu
- Tự thiết kế các cấu trúc dữ liệu cần thiết.
- Load dữ liệu từ log file (tối đa 10.000 dòng).
- Tiến hành lưu trữ và quản lý dữ liệu trên bộ nhớ.
- Tiến hành các bước tiền xử lý cần thiết như sắp xếp, đánh chỉ mục, ...
- Thực hiện khai thác hệ thống:
 Với 1 người dùng, hãy liệt kê hành trình truy xuất tài nguyên Device → App →
Resource của họ trong 1 khoảng thời gian cho trước.
 Với 1 tài nguyên, hãy liệt kê hành trình truy xuất User → Device → App trong 1
khoảng thời gian cho trước.
 Thống kê tóp 10 tài nguyên được truy xuất nhiều nhất trong khoảng thời gian cho
trước.



Đánh giá cuối kì
A. Mục tiêu
Nâng cấp Halo thành hệ thống có khả năng phát hiện bất thường trên dữ liệu lớn.
B. Thời lượng
Sinh viên hoàn tất trong 3 tuần.
C. Yêu cầu
- Load dữ liệu lớn từ log file (tối thiểu 1.000.000 dòng).
- Phát hiện bất thường dựa trên ngưỡng:
 Một người dùng đăng nhập từ quá nhiều device trong thời gian ngắn.
 Người dùng login thất bại liên tục.
 Một thiết bị đột ngột truy cập quá nhiều resource khác nhau.
 Truy cập ngoài giờ làm việc.
- Phát hiện bất thường dựa trên hành vi:
 Người dùng xuất hiện ở nhiều quốc gia không hợp lý.
 Người dùng liên tục đổi vị trí địa lý.
- Phát hiện bất thường dựa trên phiên làm việc
 Phiên làm việc dài bất thường.
 Người dùng tạo nhiều phiên làm việc liên tục bất thường.
 Phiên làm việc có chuỗi hành động nguy hiểm đáng ngờ như thực hiện các action
ở mức admin và download liên tục.
- Nâng cao
 Cố đăng nhập (đăng nhập thất bại nhiều lần liên tiếp, lần cuối thì thành công).
 Người dùng im lặng rất lâu rồi đột ngột hoạt động mạnh.


Lưu ý:
- Hệ hống cần có khả năng làm việc trên tối thiểu 1 triệu dòng dữ liệu mà không được
crash.
- Sau mỗi chức năng, bộ nhớ của hệ thống phải được giải phóng.
- Hệ thống vẫn có khả năng làm việc khi dữ liệu tồn tại giá trị không hợp lệ hoặc các
dòng dữ liệu trùng lấp.
- Hệ thống cần trả kết quả dưới 10 giây trên dữ liệu 1 triệu dòng.
- Sinh viên thực hiện đầy đủ và chính xác các yêu cầu từ 1 đến 4 được tối đa 9 điểm,
hoàn tất yêu cầu 5 được cộng thêm 2 điểm.
- Sinh viên có thể đề xuất thêm những bất thường mới có thể được phát hiện trên dữ
liệu được cung cấp. Mỗi đề xuất mới sẽ được cộng từ 0.2 đến 1 điểm tuỳ mức độ và
sức hấp dẫn.



Yêu cầu kĩ thuật
Bắt buộc sử dụng:
 Cấu trúc structure
 Con trỏ
 Cấp phát mảng động/danh sách liên kết
Không được sử dụng:
 Các lớp container đã được hổ trợ của thư viện chuẩn như: vector, map,
unordered_map, set
 Các thư viện đồ thị
Được phép sử dụng:
- Lớp std::string
- Các hàm/lớp đọc ghi file
Tất cả dữ liệu được cấp phát động đều phải được thu hồi. Việc thiếu hoặc quên được xem
như không thoả mãn yêu cầu kĩ thuật và nhận được 2 điểm cho bài làm.
Sinh viên tự thiết kế cách thức giao tiếp giữa chương trình và người sử dụng.



Quy định nộp bài
 Đồ án cá nhân.
 Tạo thư mục có tên là mã số của sinh viên thực hiện. Thư mục này gồm:
o Thư mục src chứa toàn bộ mã nguồn chương trình.
o Thư mục release chứa file thực thi được build ở chế độ release.
o Thư mục doc chứa báo cáo đánh giá mức hoàn thành đồ án, thiết kế hệ
thống, cách sử dụng, các khó khăn gặp phải và hướng giải quyết.
 Sinh viên phải xoá thư mục Debug và các file tạm sinh ra trong quá trình biên dịch
trước khi nộp bài. s
 Bài làm được nén thành file .zip hoặc .rar và nộp trên Moodle trong thời gian cho
phép. Giáo viên không nhận bài nộp trễ qua email.
 Sinh viên cần scan virus bài làm cẩn thận trước khi nộp.
 Hai bài giống nhau từ 80% trở lên đều bị xem là giống nhau và đều bị 1 điểm, bất kể
ai là tác giả.