# GPIO Event Driver

Linux Device Driver 학습을 위해 단계적으로 구현한 GPIO Event Driver 프로젝트입니다.

Character Device부터 시작하여 GPIO Interrupt, Wait Queue, Poll, ioctl, Kernel Thread, Sysfs까지 Linux 디바이스 드라이버의 핵심 기능을 직접 구현하고 실습하는 것을 목표로 하였습니다.

---

## 주요 기능

- Character Device (`/dev/gpio_event`)
- GPIO Interrupt 처리
- Software Debounce
- Event FIFO (`kfifo`)
- Blocking Read
- Poll 지원
- LED 제어
- ioctl을 통한 debounce 설정
- sysfs를 통한 debounce 설정
- Kernel Thread 기반 상태 출력

---

## 개발 환경

- Raspberry Pi
- Linux Kernel Module
- Out-of-Tree Driver

GPIO 설정:

```c
#define LED_GPIO_BCM     17
#define BUTTON_GPIO_BCM  27
```

---

## 프로젝트 구조

```text
.
├── gpio_event_driver.c
├── Makefile
├── read_event.c
├── test_debounce_ioctl.c
└── README.md
```

---

## 빌드

커널 모듈 빌드:

```bash
make
```

빌드 결과:

```text
gpio_event_driver.ko
```

빌드 산출물 제거:

```bash
make clean
```

사용자 공간 테스트 프로그램 빌드:

```bash
gcc -o read_event read_event.c
gcc -o test_debounce_ioctl test_debounce_ioctl.c
```

---

## 모듈 로드

```bash
sudo insmod gpio_event_driver.ko
```

로그 확인:

```bash
dmesg | tail
```

모듈 제거:

```bash
sudo rmmod gpio_event_driver
```

---

## GPIO 이벤트 읽기

테스트 프로그램 실행:

```bash
./read_event
```

버튼을 누르면 이벤트가 출력됩니다.

예시:

```text
waiting for gpio event...
event: seq=1 value=1
event: seq=2 value=0
```

이벤트 구조체:

```c
struct gpio_event {
    uint32_t seq;
    int value;
};
```

| 필드 | 설명 |
|---|---|
| seq | 이벤트 순번 |
| value | GPIO 입력 값 |

---

## LED 제어

LED ON:

```bash
echo 1 > /dev/gpio_event
```

LED OFF:

```bash
echo 0 > /dev/gpio_event
```

---

## Poll 지원

드라이버는 `poll()` 인터페이스를 지원합니다.

사용자 프로그램은 GPIO 이벤트가 발생할 때까지 대기하다가, 이벤트 발생 시 `read()`로 이벤트를 읽습니다.

동작 흐름:

```text
poll()
  ↓
버튼 입력 발생
  ↓
IRQ Handler
  ↓
Delayed Work
  ↓
FIFO 저장
  ↓
wake_up_interruptible()
  ↓
poll() 반환
  ↓
read()
```

---

## Debounce 설정 (ioctl)

현재 debounce 값을 조회하거나 변경할 수 있습니다.

지원 ioctl:

```c
#define GPIO_EVENT_IOC_MAGIC 'g'
#define GPIO_EVENT_IOC_SET_DEBOUNCE_MS _IOW(GPIO_EVENT_IOC_MAGIC, 1, unsigned int)
#define GPIO_EVENT_IOC_GET_DEBOUNCE_MS _IOR(GPIO_EVENT_IOC_MAGIC, 2, unsigned int)
```

현재 값 조회:

```bash
./test_debounce_ioctl
```

값 변경:

```bash
./test_debounce_ioctl 50
```

예시:

```text
current debounce_ms = 20
set debounce_ms = 50
new debounce_ms = 50
```

---

## Debounce 설정 (sysfs)

경로:

```text
/sys/class/gpio_event/gpio_event/debounce_ms
```

현재 값 확인:

```bash
cat /sys/class/gpio_event/gpio_event/debounce_ms
```

값 변경:

```bash
echo 50 | sudo tee /sys/class/gpio_event/gpio_event/debounce_ms
```

ioctl과 sysfs는 동일한 `debounce_ms` 변수를 공유합니다.

---

## Debounce 범위

```c
#define DEFAULT_DEBOUNCE_MS 20
#define MIN_DEBOUNCE_MS     0
#define MAX_DEBOUNCE_MS     1000
```

범위를 벗어난 값은 `EINVAL` 오류를 반환합니다.

---

## 내부 동작 구조

```text
GPIO Interrupt
      ↓
 IRQ Handler
      ↓
 Delayed Work
      ↓
 Debounce 처리
      ↓
 GPIO 값 읽기
      ↓
 Event FIFO 저장
      ↓
 Wait Queue Wakeup
      ↓
 read() / poll()
```

---

## 사용된 커널 기능

- Character Device
- GPIO API
- Interrupt Handler
- Delayed Workqueue
- Wait Queue
- Poll
- kfifo
- Spinlock
- ioctl
- sysfs
- Kernel Thread
- copy_to_user()
- copy_from_user()

---

## 프로젝트 목표

Linux 디바이스 드라이버의 핵심 메커니즘을 직접 구현하고 이해하는 것을 목표로 작성한 학습용 프로젝트입니다.

구현 과정에서 Character Device, GPIO Interrupt, Wait Queue, Poll, ioctl, Kernel Thread, Sysfs 등의 기능을 단계적으로 추가하며 Linux 커널 드라이버 구조를 학습하였습니다.

---

## 라이선스

GPL
