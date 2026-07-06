# GPIO Event Driver

GPIO 버튼 입력을 Linux character device로 전달하는 GPIO event driver 예제입니다.

이 프로젝트는 GPIO interrupt, debounce 처리, wait queue, `poll()`, sysfs, `ioctl`, platform driver, devm API, Device Tree, `gpiod` descriptor API를 단계적으로 적용하며 Linux device driver 구조를 학습하기 위한 프로젝트입니다.

현재 Raspberry Pi 환경에서는 Device Tree overlay 기반으로, BeagleBone Black 환경에서는 Yocto image + custom DTB + `extlinux.conf` 기반으로 동작을 확인했습니다.

## Features

- Linux kernel module 기반 GPIO event driver
- `platform_driver` 기반 `probe()` / `remove()` 구조
- Device Tree 기반 platform device 생성
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
- BeagleBone Black용 Yocto layer 제공
- BeagleBone Black용 custom DTB + `extlinux.conf` 기반 부팅 구성 제공

## Supported Boards

| Board | Integration method | Status |
|---|---|---|
| Raspberry Pi | Device Tree overlay + kernel module | Tested |
| BeagleBone Black | Yocto image + custom DTB + `extlinux.conf` | Tested |

## Hardware

현재 테스트한 GPIO 구성은 다음과 같습니다.

| Board | Function | Pin / GPIO | Direction | Active level |
|---|---|---:|---|---|
| Raspberry Pi | Button | BCM GPIO27 | Input | Active-low |
| Raspberry Pi | LED | BCM GPIO17 | Output | Active-high |
| BeagleBone Black | Button | P9_15 / GPIO48 | Input | Active-low |
| BeagleBone Black | LED | P9_12 / GPIO60 | Output | Active-high |

## Project Structure

```text
.
├── .clangd
├── .gitignore
├── Makefile
├── README.md
├── gpio-event-overlay.dts
├── gpio_event_driver.c
├── read_event.c
└── yocto
    └── meta-gpio-event-bbb
        ├── conf
        │   └── layer.conf
        ├── recipes-bsp
        │   └── gpio-event-bbb-extlinux
        │       ├── gpio-event-bbb-extlinux.bb
        │       └── files
        │           └── extlinux.conf
        ├── recipes-core
        │   └── images
        │       └── gpio-event-bbb-image.bb
        ├── recipes-kernel
        │   ├── gpio-event-bbb-driver
        │   │   ├── gpio-event-bbb-driver.bb
        │   │   └── files
        │   │       ├── Makefile
        │   │       └── gpio-event-bbb-driver.c
        │   └── linux
        │       ├── linux-yocto_%.bbappend
        │       └── files
        │           └── am335x-boneblack-gpio-event.dts
        └── recipes-support
            └── read-event
                ├── read-event.bb
                └── files
                    └── read_event.c
```

## Project Files

### Common / Raspberry Pi files

| Path | Description |
|---|---|
| `gpio_event_driver.c` | Raspberry Pi용 GPIO event kernel module source |
| `gpio-event-overlay.dts` | Raspberry Pi Device Tree overlay source |
| `read_event.c` | userspace event test program |
| `Makefile` | Raspberry Pi용 kernel module, userspace program, Device Tree overlay build rules |
| `README.md` | project documentation |

### BeagleBone Black Yocto files

| Path | Description |
|---|---|
| `yocto/meta-gpio-event-bbb/conf/layer.conf` | Yocto layer metadata |
| `yocto/meta-gpio-event-bbb/recipes-core/images/gpio-event-bbb-image.bb` | BeagleBone Black GPIO event demo image recipe |
| `yocto/meta-gpio-event-bbb/recipes-kernel/gpio-event-bbb-driver/gpio-event-bbb-driver.bb` | BBB용 GPIO event kernel module recipe |
| `yocto/meta-gpio-event-bbb/recipes-kernel/gpio-event-bbb-driver/files/gpio-event-bbb-driver.c` | BBB용 GPIO event kernel module source |
| `yocto/meta-gpio-event-bbb/recipes-kernel/gpio-event-bbb-driver/files/Makefile` | BBB kernel module build Makefile used by Yocto |
| `yocto/meta-gpio-event-bbb/recipes-kernel/linux/linux-yocto_%.bbappend` | custom BBB Device Tree source를 `linux-yocto` build에 추가하는 bbappend |
| `yocto/meta-gpio-event-bbb/recipes-kernel/linux/files/am335x-boneblack-gpio-event.dts` | BeagleBone Black custom Device Tree source |
| `yocto/meta-gpio-event-bbb/recipes-bsp/gpio-event-bbb-extlinux/gpio-event-bbb-extlinux.bb` | `extlinux.conf`를 deploy하는 recipe |
| `yocto/meta-gpio-event-bbb/recipes-bsp/gpio-event-bbb-extlinux/files/extlinux.conf` | custom DTB를 사용해 부팅하기 위한 extlinux configuration |
| `yocto/meta-gpio-event-bbb/recipes-support/read-event/read-event.bb` | userspace `read_event` utility recipe |
| `yocto/meta-gpio-event-bbb/recipes-support/read-event/files/read_event.c` | Yocto image에 포함되는 userspace event test program |

## Raspberry Pi Hardware

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

## Raspberry Pi Device Tree Overlay

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

## Raspberry Pi Build

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

## Raspberry Pi Install Device Tree Overlay

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

Device Tree와 kernel module은 역할이 다릅니다.

Raspberry Pi에서는 `dtoverlay`가 Device Tree node를 추가합니다.

```text
dtoverlay=gpio-event
        ↓
Device Tree에 gpio-event node 추가
        ↓
platform bus가 platform device 생성
        ↓
compatible = "miiniipark,gpio-event"
```

BeagleBone Black Yocto 이미지에서는 custom DTB가 부팅 시 로드되어 Device Tree node를 제공합니다.

```text
U-Boot
        ↓
extlinux.conf
        ↓
custom DTB
        ↓
Device Tree에 gpio-event node 포함
        ↓
platform bus가 platform device 생성
        ↓
compatible = "miiniipark,gpio-event"
```

드라이버 모듈이 로드되면 `of_match_table`과 Device Tree `compatible`이 매칭되어 `probe()`가 호출됩니다.

```text
gpio_event_driver.ko 또는 gpio_event_bbb_driver.ko 로드
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

자동 로드 설정을 사용하지 않는 경우, Device Tree 적용 후 직접 모듈을 로드합니다.

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

## BeagleBone Black Yocto Integration

BeagleBone Black 환경에서는 `yocto/meta-gpio-event-bbb` 레이어를 통해 GPIO event driver를 Yocto 이미지에 통합합니다.

이 구성은 커널 모듈, userspace 테스트 프로그램, BeagleBone Black용 Device Tree 구성, boot partition용 `extlinux.conf`를 이미지에 포함합니다.

### Yocto Layer Contents

`yocto/meta-gpio-event-bbb`는 BeagleBone Black에서 GPIO event driver를 이미지에 통합하기 위한 Yocto layer입니다.

```text
yocto/meta-gpio-event-bbb
├── conf
│   └── layer.conf
├── recipes-bsp
│   └── gpio-event-bbb-extlinux
│       ├── gpio-event-bbb-extlinux.bb
│       └── files
│           └── extlinux.conf
├── recipes-core
│   └── images
│       └── gpio-event-bbb-image.bb
├── recipes-kernel
│   ├── gpio-event-bbb-driver
│   │   ├── gpio-event-bbb-driver.bb
│   │   └── files
│   │       ├── Makefile
│   │       └── gpio-event-bbb-driver.c
│   └── linux
│       ├── linux-yocto_%.bbappend
│       └── files
│           └── am335x-boneblack-gpio-event.dts
└── recipes-support
    └── read-event
        ├── read-event.bb
        └── files
            └── read_event.c
```

각 구성의 역할은 다음과 같습니다.

| Component | Role |
|---|---|
| `conf/layer.conf` | Yocto layer 등록 정보, recipe search path, layer compatibility 설정 |
| `recipes-core/images/gpio-event-bbb-image.bb` | BBB용 demo image recipe. kernel module, `read_event`, custom DTB, `extlinux.conf`를 image에 포함 |
| `recipes-kernel/gpio-event-bbb-driver` | BBB용 GPIO event kernel module을 빌드하고 package로 제공 |
| `recipes-kernel/linux` | `linux-yocto` 빌드에 custom BBB DTS를 추가하고 DTB를 생성 |
| `recipes-bsp/gpio-event-bbb-extlinux` | boot partition에 들어갈 `extlinux.conf`를 deploy |
| `recipes-support/read-event` | userspace test utility를 빌드해 rootfs에 설치 |

### BeagleBone Black Hardware Configuration

| Function | BeagleBone Black Pin | GPIO | Direction | Active level |
|---|---|---:|---|---|
| Button | P9_15 | GPIO48 | Input | Active-low |
| LED | P9_12 | GPIO60 | Output | Active-high |

### BeagleBone Black Boot Flow

```text
Yocto image
        ↓
U-Boot loads extlinux.conf
        ↓
custom DTB is loaded
        ↓
Device Tree creates gpio-event platform device
        ↓
gpio-event platform driver probes
        ↓
/dev/gpio_event is created
        ↓
read_event receives button events from userspace
```

### BeagleBone Black Yocto Build Flow

```text
meta-gpio-event-bbb
        ↓
gpio-event-bbb-image.bb
        ↓
kernel module recipe
        ↓
custom Device Tree bbappend
        ↓
extlinux.conf recipe
        ↓
read_event userspace recipe
        ↓
Yocto image for BeagleBone Black
```

### BeagleBone Black Test

다음 항목을 BeagleBone Black에서 확인했습니다.

```text
- Yocto recipe parse
- kernel module build
- custom DTB build
- image dependency configuration
- BeagleBone Black boot
- /dev/gpio_event creation
- button press/release event handling
- read_event userspace event receive test
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
12. Raspberry Pi Device Tree overlay 추가
13. legacy GPIO number API에서 gpiod descriptor API로 전환
14. 내부 pull-up + active-low 버튼 회로 적용
15. Device Tree compatible 기반 module alias 및 자동 로드 흐름 확인
16. read_event 테스트 프로그램으로 Raspberry Pi 동작 검증
17. BeagleBone Black용 Yocto layer 추가
18. BeagleBone Black custom DTB + extlinux 기반 부팅 구성 추가
19. Yocto 이미지에 kernel module과 read_event utility 통합
20. BeagleBone Black에서 boot, probe, /dev/gpio_event, 버튼 이벤트 동작 검증
```

## Current Status

현재 구현은 다음 환경에서 동작 확인되었습니다.

### Raspberry Pi

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

### BeagleBone Black

```text
Button: P9_15 / GPIO48, active-low
LED:    P9_12 / GPIO60, active-high
```

검증 항목:

```text
- Yocto layer parse
- kernel module recipe build
- custom DTB build
- extlinux.conf 기반 boot configuration 적용
- Yocto image에 kernel module 포함
- Yocto image에 read_event utility 포함
- BeagleBone Black boot
- platform_driver probe 동작
- /dev/gpio_event 생성
- button press/release 이벤트 read
- read_event userspace utility 동작
```

## Notes

Raspberry Pi와 BeagleBone Black은 같은 driver binding인 `compatible = "miiniipark,gpio-event"`를 사용하지만, 보드 통합 방식은 다릅니다.

```text
Raspberry Pi
    → Device Tree overlay
    → dtoverlay=gpio-event
    → kernel module load

BeagleBone Black
    → Yocto image
    → custom DTB
    → extlinux.conf
    → kernel module included in image
```

BeagleBone Black 구성은 overlay 방식이 아니라 custom DTB와 static `extlinux.conf`를 사용하는 방식입니다.
