# GPIO Event Driver

Raspberry Pi의 GPIO 버튼 입력을 Linux character device로 전달하는 GPIO event driver 예제입니다.

이 프로젝트는 GPIO interrupt, debounce 처리, wait queue, `poll()`, sysfs, `ioctl`, platform driver, devm API, Device Tree overlay, `gpiod` descriptor API를 단계적으로 적용하며 Linux device driver 구조를 학습하기 위한 프로젝트입니다.

최종 구현은 Device Tree overlay를 통해 button/LED GPIO를 정의하고, 드라이버에서는 `devm_gpiod_get()` 기반 `gpiod` API로 GPIO를 제어합니다.

## Features

- Linux kernel module 기반 GPIO event driver
- `platform_driver` 기반 `probe()` / `remove()` 구조
- Device Tree overlay 기반 platform device 생성
- `gpiod` descriptor API 기반 GPIO 제어
- Device Tree `compatible`과 module alias 기반 자동 모듈 로드 지원
- Button GPIO interrupt 처리
- Rising/Falling edge interrupt 지원
- `delayed_work` 기반 software debounce
- 내부 FIFO 기반 버튼 이벤트 저장
- blocking / non-blocking `read()` 지원
- `poll()` / `select()` / `epoll()` 지원
- 버튼 상태에 따른 LED 상태 동기화
- `debounce_ms` sysfs attribute 지원
- `debounce_ms` `ioctl` SET/GET 지원
- 유저 공간 테스트 프로그램 `read_event` 제공

## Hardware

현재 테스트한 GPIO 구성은 다음과 같습니다.

| Function | Raspberry Pi BCM GPIO | Direction | Active level |
|---|---:|---|---|
| Button | GPIO27 | Input | Active-low |
| LED | GPIO17 | Output | Active-high |

### Button Circuit

버튼은 Raspberry Pi 내부 pull-up 저항을 사용합니다.

```text
GPIO27 ─── Button ─── GND
```

동작 의미는 다음과 같습니다.

```text
Button released → GPIO27 is pulled HIGH internally
Button pressed  → GPIO27 is connected to GND
```

Device Tree에서는 버튼을 active-low로 정의합니다.

```dts
button-gpios = <&gpio 27 1>;
```

따라서 드라이버에서 `gpiod_get_value_cansleep()`으로 읽은 logical value는 다음 의미를 가집니다.

```text
1 → Button pressed
0 → Button released
```

### LED Circuit

LED는 active-high 출력으로 구성합니다.

```text
GPIO17 ─── Resistor ─── LED ─── GND
```

동작 의미는 다음과 같습니다.

```text
GPIO17 HIGH → LED ON
GPIO17 LOW  → LED OFF
```

일반적으로 LED 직렬 저항은 `220Ω` ~ `1kΩ` 범위에서 사용합니다.

## Project Files

```text
.
├── gpio_event_driver.c
├── gpio-event-overlay.dts
├── read_event.c
├── Makefile
└── README.md
```

## Device Tree Overlay

`gpio-event-overlay.dts`는 Raspberry Pi GPIO 설정과 driver binding 정보를 정의합니다.

```dts
/dts-v1/;
/plugin/;

/ {
	compatible = "brcm,bcm2835";

	fragment@0 {
		target = <&gpio>;
		__overlay__ {
			gpio_event_button_pins: gpio-event-button-pins {
				brcm,pins = <27>;
				brcm,function = <0>; /* input */
				brcm,pull = <2>;     /* pull-up */
			};

			gpio_event_led_pins: gpio-event-led-pins {
				brcm,pins = <17>;
				brcm,function = <1>; /* output */
				brcm,pull = <0>;     /* no pull */
			};
		};
	};

	fragment@1 {
		target-path = "/";
		__overlay__ {
			gpio_event: gpio-event {
				compatible = "miiniipark,gpio-event";

				pinctrl-names = "default";
				pinctrl-0 = <&gpio_event_button_pins
					     &gpio_event_led_pins>;

				button-gpios = <&gpio 27 1>;
				led-gpios = <&gpio 17 0>;

				status = "okay";
			};
		};
	};
};
```

GPIO flag 값은 다음 의미를 가집니다.

```text
0 → GPIO_ACTIVE_HIGH
1 → GPIO_ACTIVE_LOW
```

`brcm,pull` 값은 다음 의미를 가집니다.

```text
0 → no pull
1 → pull-down
2 → pull-up
```

## Build

커널 모듈, 유저 테스트 프로그램, Device Tree overlay를 모두 빌드합니다.

```bash
make
```

빌드 결과물은 다음과 같습니다.

```text
gpio_event_driver.ko
read_event
gpio-event.dtbo
```

커널 모듈만 빌드하려면:

```bash
make module
```

유저 공간 테스트 프로그램만 빌드하려면:

```bash
make user
```

Device Tree overlay만 빌드하려면:

```bash
make dtbo
```

빌드 결과물을 정리하려면:

```bash
make clean
```

## Install Device Tree Overlay

빌드된 overlay를 Raspberry Pi overlay 디렉터리에 복사합니다.

```bash
sudo cp gpio-event.dtbo /boot/firmware/overlays/
```

`/boot/firmware/config.txt`에 다음 줄을 추가합니다.

```text
dtoverlay=gpio-event
```

예시:

```bash
sudo nano /boot/firmware/config.txt
```

수정 후 Raspberry Pi를 재부팅합니다.

```bash
sudo reboot
```

재부팅 후 overlay node가 적용되었는지 확인합니다.

```bash
ls /proc/device-tree/gpio-event
```

`compatible` 값을 확인하려면:

```bash
tr -d '\0' < /proc/device-tree/gpio-event/compatible
```

기대 출력:

```text
miiniipark,gpio-event
```

## Install Kernel Module

개발 중에는 현재 디렉터리의 `.ko` 파일을 직접 로드할 수 있습니다.

```bash
sudo insmod gpio_event_driver.ko
```

Device Tree overlay와 함께 자동 로드를 확인하려면 모듈을 커널 모듈 디렉터리에 설치하고 `depmod`를 실행합니다.

```bash
sudo mkdir -p /lib/modules/$(uname -r)/extra
sudo cp gpio_event_driver.ko /lib/modules/$(uname -r)/extra/
sudo depmod -a
```

또는 `install` 명령으로 한 번에 복사할 수 있습니다.

```bash
sudo install -D -m 644 gpio_event_driver.ko \
	/lib/modules/$(uname -r)/extra/gpio_event_driver.ko
sudo depmod -a
```

모듈 alias가 등록되었는지 확인합니다.

```bash
modinfo gpio_event_driver | grep alias
```

기대되는 alias는 다음과 같은 형태입니다.

```text
alias:          of:N*T*Cmiiniipark,gpio-eventC*
alias:          of:N*T*Cmiiniipark,gpio-event
```

이 alias는 드라이버의 `MODULE_DEVICE_TABLE(of, ...)`와 Device Tree의 `compatible = "miiniipark,gpio-event";`를 기반으로 생성됩니다.

## Module Loading Flow

`dtoverlay`와 kernel module은 역할이 다릅니다.

```text
dtoverlay=gpio-event
        ↓
Device Tree에 gpio-event node 추가
        ↓
platform bus가 platform device 생성
        ↓
compatible = "miiniipark,gpio-event"
```

드라이버 모듈이 로드되면 `of_match_table`과 Device Tree `compatible`이 매칭되어 `probe()`가 호출됩니다.

```text
gpio_event_driver.ko 로드
        ↓
platform_driver_register()
        ↓
of_match_table match
        ↓
gpio_event_probe()
        ↓
/dev/gpio_event 생성
```

모듈을 `/lib/modules/$(uname -r)/extra/`에 설치하고 `depmod -a`를 실행하면, 부팅 중 Device Tree node의 `modalias`를 기반으로 `udev`/`kmod`가 모듈을 자동 로드할 수 있습니다.

수동으로 로드할 수도 있습니다.

```bash
sudo modprobe gpio_event_driver
```

수동으로 언로드하려면:

```bash
sudo modprobe -r gpio_event_driver
```

현재 platform device의 `modalias`는 다음 명령으로 확인할 수 있습니다.

```bash
cat /sys/bus/platform/devices/gpio-event/modalias
```

## Load Kernel Module Manually

자동 로드 설정을 사용하지 않는 경우, Device Tree overlay 적용 후 직접 모듈을 로드합니다.

```bash
sudo insmod gpio_event_driver.ko
```

모듈을 설치한 뒤에는 `modprobe`로 로드할 수 있습니다.

```bash
sudo modprobe gpio_event_driver
```

커널 로그를 확인합니다.

```bash
dmesg | tail -n 30
```

정상적으로 `probe()`되면 `/dev/gpio_event`가 생성됩니다.

```bash
ls -l /dev/gpio_event
```

## Unload Kernel Module

`insmod`로 로드한 경우:

```bash
sudo rmmod gpio_event_driver
```

`modprobe`로 로드한 경우:

```bash
sudo modprobe -r gpio_event_driver
```

커널 로그를 확인합니다.

```bash
dmesg | tail -n 30
```

## User Interface

### Character Device

드라이버는 다음 character device를 생성합니다.

```text
/dev/gpio_event
```

`read()`는 버튼 상태를 1바이트 문자로 반환합니다.

```text
'1' → Button pressed
'0' → Button released
```

버튼 GPIO는 Device Tree에서 active-low로 정의되어 있으므로, 유저 공간에서는 물리 GPIO 레벨이 아니라 logical button state 기준으로 해석합니다.

### Sysfs

Debounce 시간은 sysfs attribute로 확인하거나 변경할 수 있습니다.

```text
/sys/class/gpio_event/gpio_event/debounce_ms
```

현재 debounce 값 확인:

```bash
cat /sys/class/gpio_event/gpio_event/debounce_ms
```

Debounce 값을 `50ms`로 변경:

```bash
echo 50 | sudo tee /sys/class/gpio_event/gpio_event/debounce_ms
```

지원 범위:

```text
1ms ~ 1000ms
```

기본값:

```text
20ms
```

### ioctl

드라이버는 debounce 설정을 위한 `ioctl`도 지원합니다.

```c
#define GPIO_EVENT_IOC_MAGIC            'g'
#define GPIO_EVENT_IOC_SET_DEBOUNCE_MS  _IOW(GPIO_EVENT_IOC_MAGIC, 1, unsigned int)
#define GPIO_EVENT_IOC_GET_DEBOUNCE_MS  _IOR(GPIO_EVENT_IOC_MAGIC, 2, unsigned int)
```

현재 기본 테스트 경로는 sysfs `debounce_ms`입니다.

## Test Program

`read_event`는 `/dev/gpio_event`를 열고 `poll()`로 이벤트를 기다린 뒤, `read()`로 버튼 상태를 읽어 출력합니다.

기본 실행:

```bash
./read_event
```

출력 예시:

```text
Reading button events from /dev/gpio_event
Press Ctrl+C to stop.

[2026-07-02 21:15:03.125] event=1 button_value=1 state=PRESSED
[2026-07-02 21:15:03.842] event=2 button_value=0 state=RELEASED
```

10개 이벤트만 읽기:

```bash
./read_event -n 10
```

5초 timeout 설정:

```bash
./read_event -t 5000
```

Device path 직접 지정:

```bash
./read_event -d /dev/gpio_event
```

도움말 출력:

```bash
./read_event -h
```

## Driver Flow

버튼 이벤트 처리 흐름은 다음과 같습니다.

```text
Button GPIO interrupt
        ↓
IRQ handler
        ↓
mod_delayed_work()
        ↓
debounce work
        ↓
gpiod_get_value_cansleep(button_gpiod)
        ↓
gpiod_set_value_cansleep(led_gpiod, value)
        ↓
push '1' or '0' into FIFO
        ↓
wake_up_interruptible()
        ↓
userspace poll()/read()
```

## Resource Management

드라이버는 `platform_driver` 구조를 사용합니다.

초기화 흐름:

```text
module_init
        ↓
alloc_chrdev_region()
        ↓
class_create()
        ↓
platform_driver_register()
        ↓
Device Tree compatible match
        ↓
probe()
```

해제 흐름:

```text
module_exit
        ↓
platform_driver_unregister()
        ↓
remove()
        ↓
class_destroy()
        ↓
unregister_chrdev_region()
```

`probe()` 내부에서는 devres 기반 API를 사용해 자원을 관리합니다.

```text
devm_kzalloc()
devm_gpiod_get()
devm_request_irq()
devm_add_action_or_reset()
```

`cdev`, device node, sysfs file은 명시적으로 정리합니다.

```text
device_remove_file()
device_destroy()
cdev_del()
```

IRQ와 delayed work는 cleanup action으로 묶어 정리합니다. IRQ를 먼저 해제한 뒤 delayed work를 취소하여, 해제 중 새로운 work가 다시 예약되는 상황을 방지합니다.

## Development History

이 프로젝트는 다음 순서로 구현되었습니다.

```text
1. Character device 기본 구조 구현
2. GPIO button/LED 제어 추가
3. GPIO interrupt 기반 버튼 이벤트 처리 추가
4. delayed_work 기반 debounce 처리 추가
5. FIFO와 wait queue 기반 blocking read 구현
6. poll/select/epoll 지원 추가
7. debounce_ms ioctl SET/GET 추가
8. debounce_ms sysfs attribute 추가
9. platform_driver 구조로 전환
10. devm API 기반 자원 관리 적용
11. cleanup action으로 IRQ/work 해제 경로 개선
12. Device Tree overlay 추가
13. legacy GPIO number API에서 gpiod descriptor API로 전환
14. 내부 pull-up + active-low 버튼 회로 적용
15. Device Tree compatible 기반 module alias 및 자동 로드 흐름 확인
16. read_event 테스트 프로그램으로 실제 보드 동작 검증
```

## Current Status

현재 구현은 Raspberry Pi에서 다음 구성을 기준으로 동작 확인되었습니다.

```text
Button: GPIO27, internal pull-up, active-low
LED:    GPIO17, active-high
```

검증 항목:

```text
- Device Tree overlay 적용
- platform_driver probe 동작
- module alias 기반 modprobe 로드
- Device Tree modalias 기반 자동 모듈 로드
- /dev/gpio_event 생성
- button press/release 이벤트 read
- poll 기반 이벤트 대기
- LED 상태 동기화
- sysfs debounce_ms 조회 및 변경
- module unload cleanup
```
