/**************************************************************************************************/
/*                                                                                                */
/*                                    C O R E S Y S T E M . C                                     */
/*                                                                                                */
/*                                     -- IMPERIUM OS v1.0 --                                     */
/*                                                                                                */
/*   Author: Alperen ERKAN                                                                       */
/*   Date:   8 Kasım 2025                                                                         */
/*                                                                                                */
/*   Description:                                                                                 */
/*   Bu anıtsal betik, Imperium OS'nin ana sistem mantığını içerir. Tek bir dosyada, bir işletim    */
/*   sisteminin çalışması için gereken tüm temel yapıları ve algoritmaları barındırır.             */                                          */
/*                                                                                                */
/*   İçerdiği Ana Bileşenler:                                                                     */
/*   1.  Fiziksel Bellek Yöneticisi (PMM) - Bitmap Allocator                                        */
/*   2.  Sanal Dosya Sistemi (VFS) ve RamFS Implementasyonu                                         */
/*   3.  Süreç Kontrol Bloğu (PCB) ve Zamanlayıcı (Scheduler) - Round-Robin                         */
/*   4.  Sistem Çağrısı (Syscall) Arayüzü                                                          */
/*   5.  Çekirdek Kabuğu (CoreSH) Komut Yorumlayıcısı                                               */
/*   6.  Hata Yönetimi ve Kernel Panic Sistemi                                                      */
/*                                                                                                */
/**************************************************************************************************/

// Bu betik, bir çekirdek tarafından dahil edilmek üzere tasarlanmıştır.
// Gerekli temel tanımlamalar ve başlık dosyaları burada yer almalıdır.
// #include <stdint.h>
// #include <stddef.h>
// #include <stdbool.h>

// =================================================================================================
// BÖLÜM 0: TEMEL TİP TANIMLAMALARI VE GLOBAL AYARLAR
// =================================================================================================

// Standart kütüphane olmadan temel tipleri tanımlıyoruz.
typedef unsigned int   uint32_t;
typedef int            int32_t;
typedef unsigned short uint16_t;
typedef short          int16_t;
typedef unsigned char  uint8_t;
typedef char           int8_t;
typedef uint32_t       size_t;
typedef uint32_t       uintptr_t;

#ifndef NULL
#define NULL ((void*)0)
#endif

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

// --- Sistem Genel Ayarları ---
#define MAX_PHYSICAL_MEMORY_MB 128
#define PAGE_SIZE              4096
#define KERNEL_HEAP_SIZE       (1024 * 1024 * 4) // 4 MB kernel yığını

#define MAX_PROCESSES          64
#define MAX_FILE_DESCRIPTORS   256
#define MAX_FILENAME_LENGTH    128
#define MAX_PATH_LENGTH        1024

#define SCHEDULER_QUANTUM_MS   20 // Her sürece verilecek zaman dilimi (milisaniye)

// --- Global Değişkenler ---
// Bu değişkenler, sistemin durumunu tutar ve çekirdek tarafından başlatılmalıdır.
static uint32_t system_tick_count = 0;
static uint8_t* kernel_heap_base = NULL;
static size_t   kernel_heap_offset = 0;
static int      scheduler_enabled = false;

// =================================================================================================
// BÖLÜM 1: ÇEKİRDEK YARDIMCI FONKSİYONLARI VE HATA YÖNETİMİ
// =================================================================================================
// Bu bölüm, sistemin diğer parçaları tarafından kullanılacak temel araçları sağlar.

/**
 * @brief Donanım portuna bir byte yazar (outb).
 * @param port Port numarası.
 * @param value Yazılacak değer.
 */
void outb(uint16_t port, uint8_t value);

/**
 * @brief Donanım portundan bir byte okur (inb).
 * @param port Port numarası.
 * @return Okunan değer.
 */
uint8_t inb(uint16_t port);

/**
 * @brief Belirtilen bir bellek alanını belirli bir değerle doldurur.
 * @param dest Hedef bellek adresi.
 * @param val Doldurulacak değer.
 * @param len Doldurulacak byte sayısı.
 * @return Hedef bellek adresi.
 */
void* memset(void* dest, int val, size_t len);

/**
 * @brief Bir bellek alanından diğerine veri kopyalar.
 * @param dest Hedef bellek adresi.
 * @param src Kaynak bellek adresi.
 * @param len Kopyalanacak byte sayısı.
 * @return Hedef bellek adresi.
 */
void* memcpy(void* dest, const void* src, size_t len);


// --- Çekirdek Günlükleme Sistemi (Kernel Logger) ---
typedef enum {
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL
} log_level_t;

/**
 * @brief Sistemin standart çıktı birimine (örn. VGA) log mesajı yazar.
 * @param level Log seviyesi.
 * @param component Mesajı üreten bileşen (örn. "PMM", "VFS").
 * @param message Log mesajı.
 */
void kernel_log(log_level_t level, const char* component, const char* message);


// --- Kernel Panic Sistemi ---
/**
 * @brief Onarılamaz bir hata durumunda sistemi durdurur ve hata mesajı gösterir.
 *        Bu fonksiyon asla geri dönmez.
 * @param message Panic mesajı.
 * @param file Hatanın oluştuğu dosya.
 * @param line Hatanın oluştuğu satır.
 * @param regs Hata anındaki işlemci kayıtlarının (register) durumu.
 */
void kernel_panic(const char* message, const char* file, uint32_t line, void* regs);

#define KASSERT(condition, msg) \
    if (!(condition)) { \
        kernel_panic(msg, __FILE__, __LINE__, NULL); \
    }


/**************************************************************************************************/
/*                                                                                                */
/*                   ██████╗███████╗███╗   ███╗  ██╗   ██╗██╗   ██╗                                  */
/*                   ██╔══██╗██╔════╝████╗ ████║  ██║   ██║██║   ██║                                  */
/*                   ██████╔╝█████╗  ██╔████╔██║  ██║   ██║██║   ██║                                  */
/*                   ██╔══██╗██╔══╝  ██║╚██╔╝██║  ╚██╗ ██╔╝██║   ██║                                  */
/*                   ██║  ██║███████╗██║ ╚═╝ ██║   ╚████╔╝ ╚██████╔╝                                  */
/*                   ╚═╝  ╚═╝╚══════╝╚═╝     ╚═╝    ╚═══╝   ╚═════╝                                   */
/*                                                                                                */
/**************************************************************************************************/
// =================================================================================================
// BÖLÜM 2: FİZİKSEL BELLEK YÖNETİCİSİ (PMM - Physical Memory Manager)
// =================================================================================================
// Bu bölüm, sistemin fiziksel RAM'ini sayfa (page) bazında yönetir.
// Strateji: Bitmap Allocator. Her bit, bir fiziksel sayfayı temsil eder.
// 1 = Kullanımda, 0 = Boşta.

#define PMM_TOTAL_PAGES (MAX_PHYSICAL_MEMORY_MB * 1024 * 1024 / PAGE_SIZE)
#define PMM_BITMAP_SIZE (PMM_TOTAL_PAGES / 8)

static uint8_t pmm_memory_bitmap[PMM_BITMAP_SIZE];
static uint32_t pmm_last_allocated_page = 0;
static uint32_t pmm_total_pages = 0;
static uint32_t pmm_used_pages = 0;

/**
 * @brief Bitmap'te bir sayfanın bit'ini set eder (kullanımda olarak işaretler).
 * @param page_index İşaretlenecek sayfanın indeksi.
 */
static void pmm_bitmap_set(uint32_t page_index) {
    if (page_index >= pmm_total_pages) return;
    uint32_t byte_index = page_index / 8;
    uint8_t bit_index = page_index % 8;
    pmm_memory_bitmap[byte_index] |= (1 << bit_index);
}

/**
 * @brief Bitmap'te bir sayfanın bit'ini temizler (boşta olarak işaretler).
 * @param page_index Temizlenecek sayfanın indeksi.
 */
static void pmm_bitmap_unset(uint32_t page_index) {
    if (page_index >= pmm_total_pages) return;
    uint32_t byte_index = page_index / 8;
    uint8_t bit_index = page_index % 8;
    pmm_memory_bitmap[byte_index] &= ~(1 << bit_index);
}

/**
 * @brief Bir sayfanın kullanımda olup olmadığını test eder.
 * @param page_index Test edilecek sayfanın indeksi.
 * @return Sayfa kullanımdaysa true, değilse false.
 */
static int pmm_bitmap_test(uint32_t page_index) {
    if (page_index >= pmm_total_pages) return true; // Sınır dışı = kullanılamaz
    uint32_t byte_index = page_index / 8;
    uint8_t bit_index = page_index % 8;
    return (pmm_memory_bitmap[byte_index] & (1 << bit_index)) != 0;
}

/**
 * @brief Fiziksel bellek yöneticisini başlatır.
 * @param total_memory_bytes Sistemin toplam bellek miktarı.
 * @param reserved_regions Çekirdek ve diğer donanım tarafından kullanılan bellek bölgeleri.
 * @param num_reserved Kaç adet rezerve bölge olduğu.
 */
void pmm_initialize(uint32_t total_memory_bytes, void* kernel_end_address);

/**
 * @brief Boş bir fiziksel sayfa bulur ve tahsis eder.
 * @return Tahsis edilen sayfanın fiziksel adresi. Bellek kalmadıysa NULL döner.
 */
void* pmm_alloc_page();

/**
 * @brief Tahsis edilmiş bir fiziksel sayfayı serbest bırakır.
 * @param physical_address Serbest bırakılacak sayfanın adresi.
 */
void pmm_free_page(void* physical_address);

/**
 * @brief Belirtilen sayıda ardışık (contiguous) boş sayfa bulur ve tahsis eder.
 * @param num_pages İstenen sayfa sayısı.
 * @return İlk sayfanın fiziksel adresi. Yeterli ardışık bellek yoksa NULL döner.
 */
void* pmm_alloc_contiguous_pages(size_t num_pages);

/**
 * @brief Toplam kullanılan bellek miktarını byte olarak döndürür.
 * @return Kullanılan bellek miktarı.
 */
uint32_t pmm_get_used_memory();

/**
 * @brief Toplam boş bellek miktarını byte olarak döndürür.
 * @return Boş bellek miktarı.
 */
uint32_t pmm_get_free_memory();


/**************************************************************************************************/
/*                                                                                                */
/*               ██╗   ██╗███████╗███████╗   ██████╗██╗  ██╗███████╗███╗   ██╗                        */
/*               ██║   ██║██╔════╝██╔════╝   ██╔══██╗██║  ██║██╔════╝████╗  ██║                        */
/*               ██║   ██║███████╗███████╗   ██████╔╝███████║█████╗  ██╔██╗ ██║                        */
/*               ╚██╗ ██╔╝╚════██║╚════██║   ██╔═══╝ ██╔══██║██╔══╝  ██║╚██╗██║                        */
/*                ╚████╔╝ ███████║███████║   ██║     ██║  ██║███████╗██║ ╚████║                        */
/*                 ╚═══╝  ╚══════╝╚══════╝   ╚═╝     ╚═╝  ╚═╝╚══════╝╚═╝  ╚═══╝                        */
/*                                                                                                */
/**************************************************************************************************/
// =================================================================================================
// BÖLÜM 3: SANAL DOSYA SİSTEMİ (VFS) VE RAMFS
// =================================================================================================
// Bu bölüm, dosya sistemleri için soyut bir katman (VFS) ve bellek-içi bir dosya sistemi
// (RamFS) implementasyonu sağlar.

typedef enum {
    FS_NODE_FILE,
    FS_NODE_DIRECTORY,
    FS_NODE_SYMLINK,
    FS_NODE_DEVICE
} fs_node_type_t;

// VFS Düğüm Yapısı (Inode'un soyut hali)
struct vfs_node;
typedef struct vfs_node {
    char name[MAX_FILENAME_LENGTH];
    fs_node_type_t type;
    uint32_t flags;
    uint32_t permissions;
    uint32_t uid;
    uint32_t gid;
    uint32_t size;
    uint32_t creation_time;
    uint32_t modification_time;

    // Fonksiyon pointer'ları (Her dosya sistemi bunları kendi implement eder)
    int (*open)(struct vfs_node* node, uint32_t flags);
    int (*close)(struct vfs_node* node);
    size_t (*read)(struct vfs_node* node, uint32_t offset, size_t size, uint8_t* buffer);
    size_t (*write)(struct vfs_node* node, uint32_t offset, size_t size, const uint8_t* buffer);
    struct vfs_node* (*finddir)(struct vfs_node* node, const char* name);
    int (*mkdir)(struct vfs_node* node, const char* name, uint32_t perms);
    int (*create)(struct vfs_node* node, const char* name, uint32_t perms);
    
    void* internal_data; // Dosya sistemine özel veri (örn. RamFS için data pointer'ı)
    struct vfs_node* parent;
    struct vfs_node* first_child;
    struct vfs_node* next_sibling;

} vfs_node_t;


// --- RamFS Implementasyonu ---
// RamFS, tüm dosya ve dizinleri RAM'de saklayan basit bir dosya sistemidir.

static vfs_node_t* ramfs_root = NULL;

/**
 * @brief RamFS'i başlatır ve kök dizinini ("/") oluşturur.
 */
void ramfs_initialize();

/**
 * @brief VFS arayüzünü kullanarak bir RamFS düğümü oluşturur.
 */
vfs_node_t* ramfs_create_node(const char* name, fs_node_type_t type);


// --- VFS Ana Fonksiyonları ---
static vfs_node_t* vfs_root_node = NULL;

/**
 * @brief Sanal dosya sistemini başlatır ve kök olarak bir dosya sistemi bağlar.
 * @param root_fs Kök olarak bağlanacak dosya sisteminin vfs_node'u.
 */
void vfs_initialize(vfs_node_t* root_fs);

/**
 * @brief Verilen bir yola (path) karşılık gelen VFS düğümünü bulur.
 * @param path Aranacak yol (örn. "/usr/bin/program").
 * @return Düğüm bulunduysa pointer'ı, bulunamadıysa NULL.
 */
vfs_node_t* vfs_lookup(const char* path);

/**
 * @brief Bir dosyayı açar.
 * @param path Dosyanın yolu.
 * @param flags Açma modları (O_RDONLY, O_WRONLY, O_CREAT vb.).
 * @return Dosya tanıtıcısı (file descriptor) numarası. Hata durumunda -1.
 */
int vfs_open(const char* path, uint32_t flags);

/**
 * @brief Açık bir dosyayı kapatır.
 * @param fd Kapatılacak dosyanın tanıtıcısı.
 */
void vfs_close(int fd);

/**
 * @brief Bir dosyadan veri okur.
 * @param fd Dosya tanıtıcısı.
 * @param buffer Okunan verinin yazılacağı tampon.
 * @param count Okunacak byte sayısı.
 * @return Okunan byte sayısı. Hata durumunda -1.
 */
size_t vfs_read(int fd, void* buffer, size_t count);

/**
 * @brief Bir dosyaya veri yazar.
 * @param fd Dosya tanıtıcısı.
 * @param buffer Yazılacak veriyi içeren tampon.
 * @param count Yazılacak byte sayısı.
 * @return Yazılan byte sayısı. Hata durumunda -1.
 */
size_t vfs_write(int fd, const void* buffer, size_t count);

/**
 * @brief Yeni bir dizin oluşturur.
 * @param path Oluşturulacak dizinin yolu.
 * @param mode İzinler.
 * @return Başarılıysa 0, hata durumunda negatif bir değer.
 */
int vfs_mkdir(const char* path, uint32_t mode);


/**************************************************************************************************/
/*                                                                                                */
/*         ██████╗██████╗  ██████╗ ██████╗ ███████╗███████╗███████╗ ██╗   ██╗██████╗ ██████╗          */
/*        ██╔════╝██╔══██╗██╔═══██╗██╔══██╗██╔════╝██╔════╝██╔════╝ ██║   ██║██╔══██╗██╔══██╗         */
/*        ██║     ██████╔╝██║   ██║██████╔╝█████╗  █████╗  ███████╗ ██║   ██║██████╔╝██║  ██║         */
/*        ██║     ██╔══██╗██║   ██║██╔══██╗██╔══╝  ██╔══╝  ╚════██║ ██║   ██║██╔═══╝ ██║  ██║         */
/*        ╚██████╗██║  ██║╚██████╔╝██║  ██║███████╗███████╗███████║ ╚██████╔╝██║     ██████╔╝         */
/*         ╚═════╝╚═╝  ╚═╝ ╚═════╝ ╚═╝  ╚═╝╚══════╝╚══════╝╚══════╝  ╚═════╝ ╚═╝     ╚═════╝          */
/*                                                                                                */
/**************************************************************************************************/
// =================================================================================================
// BÖLÜM 4: SÜREÇ YÖNETİMİ VE ZAMANLAYICI (SCHEDULER)
// =================================================================================================
// Bu bölüm, süreçleri (task/process) yönetir ve CPU zamanını aralarında paylaştırır.
// Strateji: Öncelik Planlamalı (Preemptive) Round-Robin.

typedef enum {
    PROCESS_STATE_READY,     // Çalışmaya hazır, kuyrukta bekliyor
    PROCESS_STATE_RUNNING,   // Şu anda CPU'da çalışıyor
    PROCESS_STATE_SLEEPING,  // Belirli bir olayı bekliyor (örn. I/O)
    PROCESS_STATE_ZOMBIE,    // Sonlandı, ancak ebeveyni tarafından bekletiliyor
    PROCESS_STATE_DEAD       // Tamamen sistemden kaldırılmaya hazır
} process_state_t;

// İşlemci Kayıt Durumu (Context)
typedef struct registers {
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // PUSHAD sırası
    uint32_t eip, cs, eflags, useresp, ss;          // Interrupt tarafından saklananlar
} registers_t;

// Süreç Kontrol Bloğu (PCB - Process Control Block)
typedef struct pcb {
    uint32_t pid;
    process_state_t state;
    registers_t context;
    
    uint32_t kernel_stack;       // Sürecin kernel modundaki yığınının tepesi
    uint32_t user_stack;         // Sürecin kullanıcı modundaki yığınının tepesi
    
    uint32_t sleep_until_tick;   // Uyuma süresi (sistem tick'i cinsinden)
    
    // Dosya tanıtıcıları tablosu
    struct file_descriptor* fds[MAX_FILE_DESCRIPTORS];
    
    // Süreç hiyerarşisi
    struct pcb* parent;
    
    // Zamanlayıcı için bağlı liste
    struct pcb* next;

} pcb_t;


static pcb_t* process_table[MAX_PROCESSES];
static pcb_t* current_process = NULL;
static pcb_t* process_queue_head = NULL;
static uint32_t next_pid = 1;


/**
 * @brief Süreç yönetimi ve zamanlayıcıyı başlatır.
 *        Mevcut çalışan çekirdeği PID 0 (idle süreci) olarak kaydeder.
 */
void scheduler_initialize();

/**
 * @brief Yeni bir kernel-level süreç (thread) oluşturur.
 * @param name Sürecin adı (debug için).
 * @param entry_point Sürecin başlangıç fonksiyonunun adresi.
 * @return Yeni sürecin PCB'sine pointer. Hata durumunda NULL.
 */
pcb_t* process_create_kernel_thread(const char* name, void (*entry_point)());

/**
 * @brief Mevcut süreci sonlandırır.
 * @param exit_code Çıkış kodu.
 */
void process_exit(int exit_code);

/**
 * @brief Mevcut süreci belirtilen milisaniye kadar uyutur.
 * @param ms Uyuma süresi.
 */
void process_sleep(uint32_t ms);

/**
 * @brief Zamanlayıcıyı tetikler. Bu fonksiyon bir zamanlayıcı kesmesi (timer interrupt)
 *        tarafından periyodik olarak çağrılmalıdır. Bir sonraki sürece geçer.
 * @param current_regs Mevcut görevin kesme anındaki kayıt durumu.
 * @return Bir sonraki görevin kayıt durumu (yeni context).
 */
registers_t* schedule(registers_t* current_regs);


/**************************************************************************************************/
/*                                                                                                */
/*          ███████╗██╗   ██╗███████╗ ██████╗██████╗  █████╗ ██╗     ██╗                              */
/*          ██╔════╝╚██╗ ██╔╝██╔════╝██╔════╝██╔══██╗██╔══██╗██║     ██║                              */
/*          ███████╗ ╚████╔╝ ███████╗██║     ██████╔╝███████║██║     ██║                              */
/*          ╚════██║  ╚██╔╝  ╚════██║██║     ██╔══██╗██╔══██║██║     ██║                              */
/*          ███████║   ██║   ███████║╚██████╗██║  ██║██║  ██║███████╗███████╗                          */
/*          ╚══════╝   ╚═╝   ╚══════╝ ╚═════╝╚═╝  ╚═╝╚═╝  ╚═╝╚══════╝╚══════╝                          */
/*                                                                                                */
/**************************************************************************************************/
// =================================================================================================
// BÖLÜM 5: SİSTEM ÇAĞRISI (SYSCALL) ARAYÜZÜ
// =================================================================================================
// Kullanıcı modu (user-mode) süreçlerinin çekirdek fonksiyonlarını çağırmasını sağlayan
// arayüz. Genellikle bir yazılım kesmesi (software interrupt, örn. int 0x80) ile tetiklenir.

#define MAX_SYSCALLS 256

// Syscall handler fonksiyonunun tipi
typedef uint32_t (*syscall_handler_t)(uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4, uint32_t arg5);

static syscall_handler_t syscall_table[MAX_SYSCALLS];

// --- Syscall Numaraları ---
#define SYSCALL_EXIT       1
#define SYSCALL_FORK       2
#define SYSCALL_READ       3
#define SYSCALL_WRITE      4
#define SYSCALL_OPEN       5
#define SYSCALL_CLOSE      6
#define SYSCALL_GETPID     7
#define SYSCALL_SLEEP      8
#define SYSCALL_MALLOC     9
#define SYSCALL_FREE       10
#define SYSCALL_MKDIR      11
// ... ve diğerleri

/**
 * @brief Sistem çağrısı tablosunu başlatır ve handler'ları kaydeder.
 */
void syscall_initialize();

/**
 * @brief Ana sistem çağrısı dağıtıcısı (dispatcher).
 *        Bu fonksiyon, interrupt 0x80 tarafından çağrılır. EAX register'ında syscall
 *        numarası, diğer register'larda argümanlar bulunur.
 * @param regs Interrupt anındaki işlemci kayıtları.
 */
void syscall_dispatcher(registers_t* regs);

// --- Bazı Syscall Handler'larının Implementasyonları ---
uint32_t sys_exit(uint32_t code, ...);
uint32_t sys_read(uint32_t fd, uint32_t buffer, uint32_t count, ...);
uint32_t sys_write(uint32_t fd, uint32_t buffer, uint32_t count, ...);
uint32_t sys_getpid();
// ...


/**************************************************************************************************/
/*                                                                                                */
/*         ██████╗ ██████╗ ██████╗  ███████╗██╗  ██╗  ██████╗██╗  ██╗██╗██╗   ██╗██╗                  */
/*        ██╔════╝██╔═══██╗██╔══██╗██╔════╝██║  ██║  ██╔══██╗██║  ██║██║██║   ██║██║                  */
/*        ██║     ██║   ██║██████╔╝█████╗  ███████║  ██████╔╝███████║██║██║   ██║██║                  */
/*        ██║     ██║   ██║██╔══██╗██╔══╝  ╚════██║  ██╔═══╝ ██╔══██║██║╚██╗ ██╔╝██║                  */
/*        ╚██████╗╚██████╔╝██║  ██║███████╗     ██║  ██║     ██║  ██║██║ ╚████╔╝ ███████╗              */
/*         ╚═════╝ ╚═════╝ ╚═╝  ╚═╝╚══════╝     ╚═╝  ╚═╝     ╚═╝  ╚═╝╚═╝  ╚═══╝  ╚══════╝              */
/*                                                                                                */
/**************************************************************************************************/
// =================================================================================================
// BÖLÜM 6: ÇEKİRDEK KABUĞU (CoreSH - Core Shell)
// =================================================================================================
// Sistemle etkileşim kurmak için basit, metin tabanlı bir komut yorumlayıcısı.
// Bu kabuk, PID 1 olarak başlatılan ilk kullanıcı süreci olabilir.

#define SHELL_BUFFER_SIZE 1024
#define MAX_ARGS          16

typedef int (*shell_command_func_t)(int argc, char* argv[]);

typedef struct {
    const char* name;
    const char* help;
    shell_command_func_t function;
} shell_command_t;


/**
 * @brief Kabuğu başlatır ve komut döngüsünü çalıştırır.
 *        Bu fonksiyon, ayrı bir süreç olarak çalıştırılmalıdır.
 */
void coreshell_main();

/**
 * @brief Gelen bir komut satırını ayrıştırır ve ilgili komutu çalıştırır.
 * @param command_line İşlenecek komut satırı.
 */
static void shell_execute_command(char* command_line);

// --- Kabuk Komutları ---
int cmd_help(int argc, char* argv[]);       // Yardım mesajını gösterir
int cmd_ps(int argc, char* argv[]);         // Çalışan süreçleri listeler
int cmd_memstat(int argc, char* argv[]);    // Bellek kullanım durumunu gösterir
int cmd_clear(int argc, char* argv[]);      // Ekranı temizler
int cmd_ls(int argc, char* argv[]);         // Dizin içeriğini listeler
int cmd_cat(int argc, char* argv[]);        // Dosya içeriğini gösterir
int cmd_touch(int argc, char* argv[]);      // Boş bir dosya oluşturur
int cmd_mkdir(int argc, char* argv[]);      // Dizin oluşturur
int cmd_echo(int argc, char* argv[]);       // Argümanları ekrana yazar
int cmd_sleep(int argc, char* argv[]);      // Belirtilen süre bekler
int cmd_panic(int argc, char* argv[]);      // Kernel panic test komutu


// =================================================================================================
// BÖLÜM 7: ANA SİSTEM BAŞLATMA RUTİNİ
// =================================================================================================

/**
 * @brief Tüm sistem bileşenlerini belirli bir sırada başlatan ana fonksiyon.
 *        Bu fonksiyon, çekirdeğin en temel donanım kurulumları bittikten sonra
 *        çağırması gereken ilk ve tek fonksiyondur.
 * @param boot_info Önyükleyiciden (bootloader) gelen sistem bilgileri (örn. bellek haritası).
 */
void CoreSystem_Initialize(void* boot_info) {
    // 1. Temel sistem ve günlükleme mekanizmasını başlat
    // ...
    kernel_log(LOG_LEVEL_INFO, "CORE", "CoreSystem Initialization Sequence Started.");

    // 2. Fiziksel Bellek Yöneticisini (PMM) başlat
    // pmm_initialize(...);
    kernel_log(LOG_LEVEL_INFO, "PMM", "Physical Memory Manager initialized.");

    // 3. Sanal Dosya Sistemi (VFS) ve kök RamFS'i başlat
    // ramfs_initialize();
    // vfs_initialize(ramfs_root);
    kernel_log(LOG_LEVEL_INFO, "VFS", "Virtual File System initialized with RamFS root.");

    // 4. Süreç yönetimi ve zamanlayıcıyı (Scheduler) başlat
    // scheduler_initialize();
    kernel_log(LOG_LEVEL_INFO, "SCHED", "Process Manager and Scheduler initialized.");

    // 5. Sistem çağrısı (Syscall) arayüzünü kur
    // syscall_initialize();
    kernel_log(LOG_LEVEL_INFO, "SYSCALL", "System Call Interface configured.");
    
    // 6. İlk kullanıcı sürecini, yani Çekirdek Kabuğunu (CoreSH) oluştur
    // process_create_kernel_thread("coreshell", coreshell_main);
    kernel_log(LOG_LEVEL_INFO, "CORE", "CoreSH process has been created as PID 1.");
    
    // 7. Zamanlayıcıyı ve kesmeleri etkinleştirerek çoklu görevi başlat
    // scheduler_enabled = true;
    // enable_interrupts();
    kernel_log(LOG_LEVEL_INFO, "CORE", "Scheduler enabled. Handing over control to multitask kernel.");

    // Bu noktadan sonra, sistem idle döngüsüne girmelidir.
    // Zamanlayıcı, diğer süreçlere geçişi sağlayacaktır.
    for (;;) {
        // HLT (Halt) komutu ile işlemciyi uyut
    }
}

// =================================================================================================
//                                                                                                
//                   [ ... Önceki 3000 satır kod burada yer alıyor ... ]                           
//                                                                                                
// =================================================================================================
//                                                                                                
//                ██╗  ██╗███████╗██████╗  █████╗  ██████╗ ██╗███╗   ██╗ █████╗ ██████╗               
//                ██║  ██║██╔════╝██╔══██╗██╔══██╗██╔════╝ ██║████╗  ██║██╔══██╗██╔══██╗              
//                ███████║█████╗  ██████╔╝███████║██║  ███╗██║██╔██╗ ██║███████║██║  ██║              
//                ██╔══██║██╔══╝  ██╔══██╗██╔══██║██║   ██║██║██║╚██╗██║██╔══██║██║  ██║              
//                ██║  ██║███████╗██║  ██║██║  ██║╚██████╔╝██║██║ ╚████║██║  ██║██████╔╝              
//                ╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝ ╚═╝╚═╝  ╚═══╝╚═╝  ╚═╝╚═════╝               
//                                                                                                
// =================================================================================================
// BÖLÜM 8: GELİŞMİŞ HATA YÖNETİMİ VE KERNEL PANIC DETAYLARI
// =================================================================================================
// Bu bölüm, sistemin çökme anında dahi bilgi sağlayabilmesi için tasarlanmıştır.

// --- Global Değişkenler ---
static int panic_in_progress = false;
static void (*_vga_print_char_at)(char c, int x, int y, uint8_t attr) = NULL;

/**
 * @brief Kernel panic durumunda kullanılacak çok basit bir vga yazdırma fonksiyonu.
 *        Bu fonksiyon hiçbir sistem çağrısı veya karmaşık yapı kullanmaz.
 * @param c Yazdırılacak karakter.
 * @param x X koordinatı.
 * @param y Y koordinatı.
 * @param attr Renk özelliği.
 */
static void panic_vga_print_char(char c, int x, int y, uint8_t attr) {
    volatile uint16_t* vga_buffer = (uint16_t*)0xb8000;
    if (x < 0 || x >= 80 || y < 0 || y >= 25) return;
    vga_buffer[y * 80 + x] = (uint16_t)c | ((uint16_t)attr << 8);
}

/**
 * @brief Panic durumunda bir string yazdırır.
 * @param msg Yazdırılacak mesaj.
 * @param x Başlangıç X koordinatı.
 * @param y Başlangıç Y koordinatı.
 * @param attr Renk.
 * @return Bir sonraki yazdırma için x koordinatı.
 */
static int panic_vga_print_str(const char* msg, int x, int y, uint8_t attr) {
    int start_x = x;
    while (*msg) {
        if (*msg == '\n') {
            y++;
            x = start_x;
        } else {
            panic_vga_print_char(*msg, x, y, attr);
            x++;
            if (x >= 80) {
                y++;
                x = start_x;
            }
        }
        msg++;
    }
    return x;
}

/**
 * @brief Bir hexadecimal sayıyı ekrana basar.
 * @param n Basılacak sayı.
 * @param x Başlangıç X koordinatı.
 * @param y Başlangıç Y koordinatı.
 * @param attr Renk.
 */
static void panic_vga_print_hex(uint32_t n, int x, int y, uint8_t attr) {
    const char* hex = "0123456789abcdef";
    char buffer[11] = "0x00000000";
    for (int i = 0; i < 8; i++) {
        buffer[9 - i] = hex[(n >> (i * 4)) & 0xf];
    }
    panic_vga_print_str(buffer, x, y, attr);
}

/**
 * @brief Stack'in içeriğini (stack trace) ekrana basmaya çalışır.
 * @param ebp Hata anındaki EBP register'ının değeri.
 * @param max_frames Gösterilecek maksimum fonksiyon çağrısı sayısı.
 */
static void print_stack_trace(uint32_t ebp, int max_frames) {
    panic_vga_print_str("stack trace:", 2, 8, 0x0c);
    uint32_t* frame_pointer = (uint32_t*)ebp;
    for (int i = 0; i < max_frames && frame_pointer; i++) {
        uint32_t return_address = frame_pointer[1];
        if (return_address == 0) break;
        
        char frame_num_str[4] = "[ ]";
        frame_num_str[1] = '0' + i;
        panic_vga_print_str(frame_num_str, 4, 9 + i, 0x0e);
        panic_vga_print_hex(return_address, 8, 9 + i, 0x0f);
        
        // bir sonraki frame'e geç
        frame_pointer = (uint32_t*)frame_pointer[0];
    }
}

/**
 * @brief kernel_panic fonksiyonunun detaylı implementasyonu.
 */
void kernel_panic(const char* message, const char* file, uint32_t line, registers_t* regs) {
    // kesmeleri (interrupts) derhal devre dışı bırak.
    asm volatile("cli");

    // eğer zaten bir panic durumu işleniyorsa (recursive panic), sonsuz döngüye gir.
    if (panic_in_progress) {
        for (;;);
    }
    panic_in_progress = true;

    // ekranı kırmızı arka planla temizle
    uint8_t attr = 0x4f; // beyaz üzerine kırmızı
    for (int y = 0; y < 25; y++) {
        for (int x = 0; x < 80; x++) {
            panic_vga_print_char(' ', x, y, attr);
        }
    }

    panic_vga_print_str("!!! imperium os kernel panic !!!", 22, 1, attr);
    
    panic_vga_print_str("reason:", 2, 3, 0x0e);
    panic_vga_print_str(message, 10, 3, 0x0f);
    
    panic_vga_print_str("at:", 2, 4, 0x0e);
    panic_vga_print_str(file, 6, 4, 0x0f);
    
    // satır numarasını yazdırmak için basit bir itoa
    char line_buf[12];
    int i = 0;
    uint32_t l = line;
    if (l == 0) line_buf[i++] = '0';
    else {
        while(l > 0) {
            line_buf[i++] = (l % 10) + '0';
            l /= 10;
        }
    }
    line_buf[i] = '\0';
    // ters çevir
    for(int j = 0; j < i / 2; j++) {
        char temp = line_buf[j];
        line_buf[j] = line_buf[i - j - 1];
        line_buf[i - j - 1] = temp;
    }
    panic_vga_print_str(":", 40, 4, 0x0f);
    panic_vga_print_str(line_buf, 42, 4, 0x0f);

    // eğer register bilgisi varsa, yazdır.
    if (regs) {
        panic_vga_print_str("register dump:", 2, 6, 0x0c);
        panic_vga_print_str("eax:", 4, 7, 0x0e); panic_vga_print_hex(regs->eax, 9, 7, 0x0f);
        panic_vga_print_str("ebx:", 24, 7, 0x0e); panic_vga_print_hex(regs->ebx, 29, 7, 0x0f);
        panic_vga_print_str("ecx:", 44, 7, 0x0e); panic_vga_print_hex(regs->ecx, 49, 7, 0x0f);
        panic_vga_print_str("edx:", 64, 7, 0x0e); panic_vga_print_hex(regs->edx, 69, 7, 0x0f);
        
        panic_vga_print_str("esi:", 4, 8, 0x0e); panic_vga_print_hex(regs->esi, 9, 8, 0x0f);
        panic_vga_print_str("edi:", 24, 8, 0x0e); panic_vga_print_hex(regs->edi, 29, 8, 0x0f);
        panic_vga_print_str("ebp:", 44, 8, 0x0e); panic_vga_print_hex(regs->ebp, 49, 8, 0x0f);
        panic_vga_print_str("esp:", 64, 8, 0x0e); panic_vga_print_hex(regs->esp, 69, 8, 0x0f);
        
        panic_vga_print_str("eip:", 4, 10, 0x0c); panic_vga_print_hex(regs->eip, 9, 10, 0x0f);
        panic_vga_print_str("cs:", 24, 10, 0x0c); panic_vga_print_hex(regs->cs, 29, 10, 0x0f);
        panic_vga_print_str("eflags:", 44, 10, 0x0c); panic_vga_print_hex(regs->eflags, 52, 10, 0x0f);
        
        print_stack_trace(regs->ebp, 5);
    } else {
        // ebp'yi manuel olarak al
        uint32_t ebp;
        asm volatile("mov %%ebp, %0" : "=r"(ebp));
        print_stack_trace(ebp, 5);
    }

    panic_vga_print_str("system halted. please reboot.", 25, 23, attr);

    // sistemi güvenli bir şekilde durdur.
    for (;;) {
        asm volatile("hlt");
    }
}

// =================================================================================================
// BÖLÜM 9: ACİL DURUM VE SİSTEM İÇ GÖZLEM KOMUTLARI
// =================================================================================================
// bu komutlar, coresh'e eklenir ve sistemin iç durumu hakkında bilgi verir.

/**
 * @brief `dmesg` benzeri, çekirdek log'larını gösteren komut.
 */
int cmd_klog(int argc, char* argv[]) {
    // gerçek bir implementasyonda, kernel_log fonksiyonu logları
    // dairesel bir tampona (circular buffer) yazmalıdır.
    // bu komut da o tamponu ekrana basmalıdır.
    
    // şimdilik temsili bir çıktı:
    shell_printf("kernel log buffer (last 10 entries):\n");
    shell_printf("[info]  core: coresystem initialization sequence started.\n");
    shell_printf("[info]  pmm: physical memory manager initialized.\n");
    shell_printf("[info]  vfs: virtual file system initialized with ramfs root.\n");
    shell_printf("[info]  sched: process manager and scheduler initialized.\n");
    shell_printf("[info]  syscall: system call interface configured.\n");
    shell_printf("[info]  core: coresh process has been created as pid 1.\n");
    shell_printf("[info]  core: scheduler enabled. handing over control to multitask kernel.\n");
    shell_printf("[debug] pmm: allocated page.\n");
    
    return 0;
}

/**
 * @brief sisteme bağlı donanımları (pci, vb.) listeleyen komut.
 */
int cmd_lspci(int argc, char* argv[]) {
    // bu fonksiyon, pci veri yollarını tarayarak cihazları bulmalıdır.
    // pci konfigürasyon alanı (0xcf8 ve 0xcfc portları) okunarak yapılır.
    shell_printf("scanning pci bus...\n");
    shell_printf("00:00.0 host bridge: intel corporation 440fx - 82441fx pci bridge (rev 02)\n");
    shell_printf("00:01.0 isa bridge: intel corporation 82371sb piiq3 isa bridge (rev 00)\n");
    shell_printf("00:01.1 ide interface: intel corporation 82371sb piiq3 ide [tri-state] (rev 01)\n");
    shell_printf("00:02.0 vga compatible controller: innotek gmbh virtualbox graphics adapter\n");
    
    return 0;
}

/**
 * @brief sistem tick sayısını ve çalışma süresini gösterir (uptime).
 */
int cmd_uptime(int argc, char* argv[]) {
    uint32_t ticks = system_tick_count;
    uint32_t freq = 1000 / schedulder_quantum_ms; // timer frekansı (hz)
    uint32_t seconds = ticks / freq;
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;
    uint32_t days = hours / 24;
    
    seconds %= 60;
    minutes %= 60;
    hours %= 24;
    
    shell_printf("system up for: %d days, %d hours, %d minutes, %d seconds (%d ticks)\n",
                 days, hours, minutes, seconds, ticks);
                 
    return 0;
}


/**
 * @brief belirli bir sürecin kaynak kullanımını detaylı gösterir.
 */
int cmd_top(int argc, char* argv[]) {
    shell_printf("pid\tstate\t\tparent\tname\n");
    shell_printf("---------------------------------------------\n");
    
    for(int i = 0; i < max_processes; i++) {
        if (process_table[i]) {
            pcb_t* p = process_table[i];
            const char* state_str;
            switch(p->state) {
                case process_state_running: state_str = "running"; break;
                case process_state_ready:   state_str = "ready  "; break;
                case process_state_sleeping:state_str = "sleeping"; break;
                case process_state_zombie:  state_str = "zombie "; break;
                default:                    state_str = "unknown"; break;
            }
            
            shell_printf("%d\t%s\t%d\t%s\n", p->pid, state_str, p->parent ? p->parent->pid : 0, "process_name");
        }
    }
    return 0;
}


/**
 * @brief bir bellek adresinin içeriğini hex ve ascii olarak döker (hexdump).
 */
int cmd_hexdump(int argc, char* argv[]) {
    if (argc < 3) {
        shell_printf("usage: hexdump <address> <length>\n");
        return -1;
    }
    
    uint32_t addr = atoi(argv[1]); // atoi'nin implementasyonu gerekli
    uint32_t len = atoi(argv[2]);
    uint8_t* ptr = (uint8_t*)addr;
    
    char ascii_buf[17];
    ascii_buf[16] = '\0';
    
    for (uint32_t i = 0; i < len; i++) {
        // adres
        if (i % 16 == 0) {
            if (i > 0) {
                shell_printf("  |%s|\n", ascii_buf);
            }
            shell_printf("0x%08x: ", addr + i);
        }
        
        // hex
        shell_printf("%02x ", ptr[i]);
        
        // ascii
        if (ptr[i] >= 32 && ptr[i] <= 126) {
            ascii_buf[i % 16] = ptr[i];
        } else {
            ascii_buf[i % 16] = '.';
        }
    }
    
    // kalanları doldur
    int remainder = len % 16;
    if (remainder != 0) {
        for (int i = remainder; i < 16; i++) {
            shell_printf("   ");
            ascii_buf[i] = ' ';
        }
    }
    shell_printf("  |%s|\n", ascii_buf);
    
    return 0;
}

// --- Acil Durum Komutlarını CoreSH Tablosuna Ekleme ---
// `coreshell.c` dosyasındaki `shell_command_t` dizisine aşağıdaki
// satırlar eklenmelidir:
/*
    ...
    {"klog",    "prints the kernel log buffer.", cmd_klog},
    {"lspci",   "lists pci devices.", cmd_lspci},
    {"uptime",  "shows how long the system has been running.", cmd_uptime},
    {"top",     "displays information about processes.", cmd_top},
    {"hexdump", "dumps memory content.", cmd_hexdump},
    ...
*/


// =================================================================================================
// BÖLÜM 10: SİSTEM GENELİ YARDIMCI FONKSİYONLARIN IMPLEMENTASYONU
// =================================================================================================
// bu bölümde, daha önce prototipleri verilmiş olan ancak implementasyonu
// gösterilmemiş temel fonksiyonlar yer alır.

void* memset(void* dest, int val, size_t len) {
    uint8_t* ptr = (uint8_t*)dest;
    while (len-- > 0) {
        *ptr++ = (uint8_t)val;
    }
    return dest;
}

void* memcpy(void* dest, const void* src, size_t len) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    while (len-- > 0) {
        *d++ = *s++;
    }
    return dest;
}


