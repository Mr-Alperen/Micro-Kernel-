; ==================================================================================================
;
;                                     M A I N . C O R E . A S M
;
;                                       -- IMPERIUM OS v1.0 --
;
;   Author:      Alperen ERKAN
;   Date:        8 Kasım 2025
;   Architecture:IA-32 (x86, 32-bit)
;   Syntax:      NASM (Netwide Assembler)
;   Target:      Intel Pentium II ve üstü
;
;   Description:
;   Bu, Imperium OS'nin ana çekirdek başlangıç betiğidir. GRUB gibi bir Multiboot uyumlu
;   önyükleyici tarafından belleğe yüklendikten sonra çalıştırılan ilk koddur.
;
;   GÖREVLERİ:
;   1. Multiboot standardına uygun bir başlık sağlamak.
;   2. 32-bit Korumalı Mod için temel ortamı kurmak:
;      - Global Descriptor Table (GDT) oluşturmak ve yüklemek.
;      - Interrupt Descriptor Table (IDT) oluşturmak ve yüklemek.
;   3. CPU istisnaları (exceptions) ve donanım kesmeleri (interrupts) için temel
;      hizmet rutinlerini (ISR) kurmak.
;   4. 8259 Programlanabilir Kesme Kontrolcüsünü (PIC) yeniden programlamak.
;   5. Yüksek seviyeli bir dilde yazılmış ana çekirdek fonksiyonuna (`kmain`)
;      kontrolü devretmek.
;
; ==================================================================================================

; ##################################################################################################
; # BÖLÜM 1: MULTIBOOT BAŞLIĞI
; ##################################################################################################
; GRUB'un çekirdeğimizi tanıması ve doğru şekilde yüklemesi için bu başlık zorunludur.

section .multiboot
align 4

; --- Multiboot Sihirli Değerler ---
MULTIBOOT_HEADER_MAGIC      equ 0x1BADB002  ; GRUB'un aradığı sihirli sayı.
MULTIBOOT_HEADER_FLAGS      equ 0x00000003  ; Bayraklar:
                                            ; Bit 0: Sayfa hizalamalı modüller.
                                            ; Bit 1: GRUB'dan bellek haritası iste.
MULTIBOOT_HEADER_CHECKSUM   equ -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)

; --- Başlık Yapısı ---
dd MULTIBOOT_HEADER_MAGIC       ; GRUB'un doğrulaması için.
dd MULTIBOOT_HEADER_FLAGS       ; GRUB'a yeteneklerimizi ve isteklerimizi bildirir.
dd MULTIBOOT_HEADER_CHECKSUM    ; Sağlama toplamı. (magic + flags + checksum) == 0 olmalı.


; ##################################################################################################
; # BÖLÜM 2: BAŞLANGIÇ NOKTASI VE ÇEKİRDEK GİRİŞİ
; ##################################################################################################
; Bu bölüm, GRUB kontrolü devrettikten sonra çalıştırılan ilk koddur.

section .text
global _start                   ; Linker için başlangıç noktasını global yap.
extern kmain                    ; Yüksek seviyeli C çekirdeğimizin fonksiyonu.

_start:
    ; CPU şu anda 32-bit Korumalı Mod'da. CS, DS, SS gibi segment register'ları
    ; GRUB tarafından kurulmuş geçici bir GDT'yi gösteriyor. Kendi GDT'mizi
    ; kurmadan önce kesmeleri kapatmak en güvenlisidir.
    cli                         ; Tüm kesmeleri devre dışı bırak.

    ; --- Çekirdek Yığınını (Kernel Stack) Kur ---
    ; Belleğin daha güvenli bir bölgesinde kendi yığınımızı oluşturuyoruz.
    ; `kernel_stack_space` .bss bölümünde rezerve edilmiştir.
    mov esp, kernel_stack_top   ; Yığın işaretçisini (ESP) yeni yığının tepesine ayarla.

    ; --- Donanım ve Ortamı Başlat ---
    ; Bu fonksiyonlar, bu dosyanın ilerleyen kısımlarında tanımlanmıştır.
    call gdt_install            ; Kendi GDT'mizi kur ve yükle.
    call idt_install            ; IDT'mizi kur ve yükle.

    ; --- Yüksek Seviyeli Çekirdeğe Zıpla ---
    ; Ortam hazır. Artık C dilinde yazılmış ana çekirdek fonksiyonumuzu çağırabiliriz.
    ; GRUB, Multiboot bilgi yapısının adresini EBX register'ında bırakır.
    ; Bunu C koduna argüman olarak iletiyoruz.
    push ebx                    ; Multiboot info struct'ı yığına koy.
    call kmain                  ; `kmain(multiboot_info* mbd)` çağrısı.
    
    ; --- Güvenlik Döngüsü ---
    ; Eğer `kmain` fonksiyonu bir sebepten ötürü geri dönerse (ki dönmemeli),
    ; sistemi güvenli bir şekilde durdurmak için sonsuz bir döngüye gir.
.hang:
    hlt                         ; İşlemciyi durdur (bir sonraki kesmeye kadar).
    jmp .hang                   ; Döngüye devam et.


; ##################################################################################################
; # BÖLÜM 3: GLOBAL DESCRIPTOR TABLE (GDT) KURULUMU
; ##################################################################################################
; GDT, Korumalı Mod'da bellek segmentasyonunu tanımlar. Kod ve veri için ayrı
; segmentler oluşturacağız.

; --- GDT Yapısı ---
gdt_start:
    ; GDT'nin ilk girdisi her zaman NULL (sıfır) olmalıdır.
    gdt_null:
        dd 0x0                  ; 8 byte'lık boş girdi.
        dd 0x0

    ; GDT Girdi 1: Kernel Kodu Segmenti (Code Segment)
    ; Base=0, Limit=4GB, Ring 0 (en yetkili seviye)
    gdt_code:
        dw 0xFFFF               ; Limit (0-15)
        dw 0x0000               ; Base (0-15)
        db 0x00                 ; Base (16-23)
        db 0x9A                 ; Access Byte: Present(1), Ring 0(00), Type(1), Code(1), Dir(0), R(1), Acc(0)
        db 0xCF                 ; Flags & Limit (16-19): Gran(1), Size(1), 0, 0, Limit(1111)
        db 0x00                 ; Base (24-31)

    ; GDT Girdi 2: Kernel Veri Segmenti (Data Segment)
    ; Base=0, Limit=4GB, Ring 0
    gdt_data:
        dw 0xFFFF               ; Limit (0-15)
        dw 0x0000               ; Base (0-15)
        db 0x00                 ; Base (16-23)
        db 0x92                 ; Access Byte: Present(1), Ring 0(00), Type(1), Data(0), Exp(0), W(1), Acc(0)
        db 0xCF                 ; Flags & Limit (16-19): Gran(1), Size(1), 0, 0, Limit(1111)
        db 0x00                 ; Base (24-31)
gdt_end:

; --- GDT Pointer Yapısı (GDTR) ---
; `lgdt` komutunun ihtiyaç duyduğu 6 byte'lık yapı.
gdt_ptr:
    dw gdt_end - gdt_start - 1  ; GDT'nin limiti (boyutu - 1).
    dd gdt_start                ; GDT'nin başlangıç adresi.

; --- GDT Kurulum Fonksiyonu ---
gdt_install:
    ; 1. GDT Pointer'ını CPU'ya yükle.
    lgdt [gdt_ptr]

    ; 2. Segment register'larını güncelle.
    ;    GDT'yi değiştirdikten sonra, segment register'larını yeniden yükleyerek
    ;    CPU'nun yeni segmentleri kullanmasını sağlamalıyız.
    ;    Bu, bir "far jump" ile yapılır.
    mov ax, 0x10                ; 0x10, veri segmentimizin GDT'deki ofsetidir.
                                ; (NULL=0x00, CODE=0x08, DATA=0x10)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; 3. Kod segmentini (CS) güncellemek için far jump yap.
    ;    Bu komut, hem CS register'ını günceller hem de işlemcinin komut
    ;    önbelleğini (prefetch queue) temizler.
    jmp 0x08:.flush_gdt         ; 0x08, kod segmentimizin ofsetidir.
.flush_gdt:
    ret                         ; GDT kurulumu tamamlandı.


; ##################################################################################################
; # BÖLÜM 4: INTERRUPT DESCRIPTOR TABLE (IDT) KURULUMU
; ##################################################################################################
; IDT, işlemcinin bir kesme veya istisna geldiğinde hangi kodu çalıştıracağını
; bilmesini sağlar. Toplam 256 adet girdi tanımlayacağız.

; --- Harici Fonksiyonlar (C'de tanımlı) ---
extern fault_handler            ; Tüm CPU istisnalarını yönetecek C fonksiyonu.
extern irq_handler              ; Tüm donanım kesmelerini yönetecek C fonksiyonu.

; --- IDT Kurulum Fonksiyonu ---
idt_install:
    ; 1. IDT Pointer'ını (IDTR) ayarla.
    mov eax, idt_ptr            ; IDT pointer'ının adresini EAX'a al.
    mov [eax], word 256 * 8 - 1 ; IDT'nin limiti: (256 girdi * 8 byte) - 1.
    mov edx, idt_start          ; IDT'nin başlangıç adresini EDX'e al.
    mov [eax + 2], edx          ; Adresi pointer yapısına yaz.

    ; 2. IDT'yi ISR (Interrupt Service Routine) ve IRQ (Interrupt ReQuest)
    ;    stub'ları ile doldur. Bu, makrolar kullanılarak yapılır.
    call populate_idt

    ; 3. PIC'i (Programmable Interrupt Controller) yeniden haritala.
    ;    Varsayılan olarak, PIC kesmeleri (IRQ 0-15), CPU'nun istisnaları
    ;    (0-15) ile çakışır. Bu çakışmayı önlemek için PIC'i yeniden programlıyoruz.
    call pic_remap

    ; 4. IDT'yi yükle ve kesmeleri etkinleştir.
    lidt [idt_ptr]              ; CPU'ya yeni IDT'mizin yerini söyle.
    sti                         ; Kesmeleri genel olarak etkinleştir.
    
    ret

; --- IDT Girdisi (Gate) Ayarlama Makrosu ---
; Bu makro, bir IDT girdisini ayarlamayı kolaylaştırır.
; Argümanlar:
;   %1: Interrupt numarası (0-255)
;   %2: Çalıştırılacak kodun adresi (ISR stub'ı)
;   %3: Kod segment seçicisi (0x08)
;   %4: Gate özellikleri (örn: 0x8E = Present, Ring 0, 32-bit Interrupt Gate)
%macro SET_IDT_GATE 4
    mov edx, %2                 ; ISR adresi
    mov [idt_start + %1 * 8], dx
    mov word [idt_start + %1 * 8 + 2], %3
    mov byte [idt_start + %1 * 8 + 4], 0
    mov byte [idt_start + %1 * 8 + 5], %4
    mov dx, edx
    shr edx, 16
    mov [idt_start + %1 * 8 + 6], dx
%endmacro

; --- IDT Doldurma Fonksiyonu ---
populate_idt:
    ; ISR'ler (CPU istisnaları, 0-31)
    SET_IDT_GATE 0,  isr0,  0x08, 0x8E
    SET_IDT_GATE 1,  isr1,  0x08, 0x8E
    SET_IDT_GATE 2,  isr2,  0x08, 0x8E
    SET_IDT_GATE 3,  isr3,  0x08, 0x8E
    SET_IDT_GATE 4,  isr4,  0x08, 0x8E
    SET_IDT_GATE 5,  isr5,  0x08, 0x8E
    SET_IDT_GATE 6,  isr6,  0x08, 0x8E
    SET_IDT_GATE 7,  isr7,  0x08, 0x8E
    SET_IDT_GATE 8,  isr8,  0x08, 0x8E
    SET_IDT_GATE 9,  isr9,  0x08, 0x8E
    SET_IDT_GATE 10, isr10, 0x08, 0x8E
    SET_IDT_GATE 11, isr11, 0x08, 0x8E
    SET_IDT_GATE 12, isr12, 0x08, 0x8E
    SET_IDT_GATE 13, isr13, 0x08, 0x8E
    SET_IDT_GATE 14, isr14, 0x08, 0x8E
    SET_IDT_GATE 15, isr15, 0x08, 0x8E
    SET_IDT_GATE 16, isr16, 0x08, 0x8E
    SET_IDT_GATE 17, isr17, 0x08, 0x8E
    SET_IDT_GATE 18, isr18, 0x08, 0x8E
    SET_IDT_GATE 19, isr19, 0x08, 0x8E
    SET_IDT_GATE 20, isr20, 0x08, 0x8E
    SET_IDT_GATE 21, isr21, 0x08, 0x8E
    SET_IDT_GATE 22, isr22, 0x08, 0x8E
    SET_IDT_GATE 23, isr23, 0x08, 0x8E
    SET_IDT_GATE 24, isr24, 0x08, 0x8E
    SET_IDT_GATE 25, isr25, 0x08, 0x8E
    SET_IDT_GATE 26, isr26, 0x08, 0x8E
    SET_IDT_GATE 27, isr27, 0x08, 0x8E
    SET_IDT_GATE 28, isr28, 0x08, 0x8E
    SET_IDT_GATE 29, isr29, 0x08, 0x8E
    SET_IDT_GATE 30, isr30, 0x08, 0x8E
    SET_IDT_GATE 31, isr31, 0x08, 0x8E

    ; IRQ'lar (Donanım kesmeleri, 32-47)
    SET_IDT_GATE 32, irq0,  0x08, 0x8E  ; Zamanlayıcı (Timer)
    SET_IDT_GATE 33, irq1,  0x08, 0x8E  ; Klavye
    SET_IDT_GATE 34, irq2,  0x08, 0x8E
    SET_IDT_GATE 35, irq3,  0x08, 0x8E
    SET_IDT_GATE 36, irq4,  0x08, 0x8E
    SET_IDT_GATE 37, irq5,  0x08, 0x8E
    SET_IDT_GATE 38, irq6,  0x08, 0x8E
    SET_IDT_GATE 39, irq7,  0x08, 0x8E
    SET_IDT_GATE 40, irq8,  0x08, 0x8E
    SET_IDT_GATE 41, irq9,  0x08, 0x8E
    SET_IDT_GATE 42, irq10, 0x08, 0x8E
    SET_IDT_GATE 43, irq11, 0x08, 0x8E
    SET_IDT_GATE 44, irq12, 0x08, 0x8E
    SET_IDT_GATE 45, irq13, 0x08, 0x8E
    SET_IDT_GATE 46, irq14, 0x08, 0x8E  ; IDE Hard disk
    SET_IDT_GATE 47, irq15, 0x08, 0x8E
    ret

; --- PIC Yeniden Haritalama (Remapping) ---
PIC1_COMMAND    equ 0x20
PIC1_DATA       equ 0x21
PIC2_COMMAND    equ 0xA0
PIC2_DATA       equ 0xA1
PIC_EOI         equ 0x20        ; End-Of-Interrupt komutu

pic_remap:
    ; Başlatma komutunu (ICW1) her iki PIC'e de gönder
    out PIC1_COMMAND, 0x11
    out PIC2_COMMAND, 0x11

    ; ICW2: Master PIC'in IRQ'larını 0x20 (32)'den başlat
    out PIC1_DATA, 0x20
    ; ICW2: Slave PIC'in IRQ'larını 0x28 (40)'tan başlat
    out PIC2_DATA, 0x28

    ; ICW3: Master PIC'e, Slave'in IRQ 2'ye bağlı olduğunu söyle
    out PIC1_DATA, 0x04
    ; ICW3: Slave PIC'e, kendisinin Master'ın 2. pini olduğunu söyle
    out PIC2_DATA, 0x02

    ; ICW4: 8086 modu
    out PIC1_DATA, 0x01
    out PIC2_DATA, 0x01

    ; Tüm kesmeleri maskele (şimdilik)
    out PIC1_DATA, 0xFF
    out PIC2_DATA, 0xFF
    ret


; ##################################################################################################
; # BÖLÜM 5: ISR ve IRQ STUB'LARI
; ##################################################################################################
; Her bir IDT girdisi, buradaki küçük bir Assembly "stub" fonksiyonuna işaret eder.
; Bu stub'lar, işlemci durumunu kaydeder ve ortak bir C handler'ını çağırır.

; --- Hata Kodu Olmayan ISR'ler için Makro ---
%macro ISR_NOERR_STUB 1
global isr%1
isr%1:
    cli                     ; Kesmeleri kapat
    push byte 0             ; Hata kodu olmayanlar için sahte hata kodu
    push byte %1            ; Interrupt numarasını yığına koy
    jmp isr_common_stub
%endmacro

; --- Hata Kodu Olan ISR'ler için Makro ---
%macro ISR_ERR_STUB 1
global isr%1
isr%1:
    cli
    ; Gerçek hata kodu zaten işlemci tarafından yığına konuldu
    push byte %1
    jmp isr_common_stub
%endmacro

; --- IRQ'lar için Makro ---
%macro IRQ_STUB 2
global irq%1
irq%1:
    cli
    push byte 0             ; Sahte hata kodu
    push byte %2            ; Interrupt numarasını (32 + IRQ no) yığına koy
    jmp irq_common_stub
%endmacro


; --- ISR Stub'ları (0-31) ---
ISR_NOERR_STUB 0
ISR_NOERR_STUB 1
ISR_NOERR_STUB 2
ISR_NOERR_STUB 3
ISR_NOERR_STUB 4
ISR_NOERR_STUB 5
ISR_NOERR_STUB 6
ISR_NOERR_STUB 7
ISR_ERR_STUB   8
ISR_NOERR_STUB 9
ISR_ERR_STUB   10
ISR_ERR_STUB   11
ISR_ERR_STUB   12
ISR_ERR_STUB   13
ISR_ERR_STUB   14
ISR_NOERR_STUB 15
ISR_NOERR_STUB 16
ISR_ERR_STUB   17
ISR_NOERR_STUB 18
ISR_NOERR_STUB 19
ISR_NOERR_STUB 20
ISR_ERR_STUB   21
ISR_NOERR_STUB 22
ISR_NOERR_STUB 23
ISR_NOERR_STUB 24
ISR_NOERR_STUB 25
ISR_NOERR_STUB 26
ISR_NOERR_STUB 27
ISR_ERR_STUB   28
ISR_ERR_STUB   29
ISR_ERR_STUB   30
ISR_ERR_STUB   31

; --- IRQ Stub'ları (0-15) ---
IRQ_STUB 0, 32
IRQ_STUB 1, 33
IRQ_STUB 2, 34
IRQ_STUB 3, 35
IRQ_STUB 4, 36
IRQ_STUB 5, 37
IRQ_STUB 6, 38
IRQ_STUB 7, 39
IRQ_STUB 8, 40
IRQ_STUB 9, 41
IRQ_STUB 10, 42
IRQ_STUB 11, 43
IRQ_STUB 12, 44
IRQ_STUB 13, 45
IRQ_STUB 14, 46
IRQ_STUB 15, 47

; --- Ortak ISR Stub'ı ---
isr_common_stub:
    pushad                  ; Tüm genel amaçlı register'ları (EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI) kaydet
    mov ax, ds
    push eax                ; Veri segmentini kaydet

    mov ax, 0x10            ; Kernel veri segmentini yükle
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov eax, esp            ; Yığın işaretçisinin adresini C fonksiyonuna argüman olarak ver
    push eax
    call fault_handler      ; C'deki handler'ı çağır
    pop eax

    pop ebx                 ; Veri segmentini geri yükle (EBX'e attık)
    mov ds, ebx
    mov es, ebx
    mov fs, ebx
    mov gs, ebx

    popad                   ; Tüm register'ları geri yükle
    add esp, 8              ; Interrupt numarasını ve hata kodunu yığından temizle
    iret                    ; Interrupt'tan dön

; --- Ortak IRQ Stub'ı ---
irq_common_stub:
    pushad
    mov ax, ds
    push eax

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov eax, esp
    push eax
    call irq_handler        ; C'deki IRQ handler'ını çağır
    pop eax

    pop ebx
    mov ds, ebx
    mov es, ebx
    mov fs, ebx
    mov gs, ebx

    popad
    add esp, 8
    iret


; ##################################################################################################
; # BÖLÜM 6: DONANIM ABSTRAKSİYON KATMANI (HAL) YARDIMCILARI
; ##################################################################################################
; C kodundan çağrılabilecek, Assembly gerektiren basit donanım erişim fonksiyonları.

global outb
outb:
    ; Argümanlar yığından alınır: [ESP+4] = port, [ESP+8] = value
    mov al, [esp + 8]       ; Değeri AL'ye al
    mov dx, [esp + 4]       ; Port'u DX'e al
    out dx, al              ; Yazma işlemini yap
    ret

global inb
inb:
    ; Argüman: [ESP+4] = port
    mov dx, [esp + 4]       ; Port'u DX'e al
    in al, dx               ; Okuma işlemini yap
    ret                     ; Değer AL register'ında döner

global enable_interrupts
enable_interrupts:
    sti
    ret

global disable_interrupts
disable_interrupts:
    cli
    ret

global halt_cpu
halt_cpu:
    hlt
    ret

; ##################################################################################################
; # BÖLÜM 7: VERİ VE BSS BÖLÜMLERİ
; ##################################################################################################
; Bu bölümde, başlatılmış (initialized) ve başlatılmamış (uninitialized) global
; değişkenler tanımlanır.

section .data
align 4
; IDT Pointer'ı
idt_ptr:
    dw 0
    dd 0


section .bss
align 4

; IDT (256 girdi * 8 byte = 2048 byte)
idt_start:
    resb 256 * 8

; Çekirdek Yığını (Kernel Stack) - 16 KB
kernel_stack_bottom:
    resb 16384 ; 16 * 1024
kernel_stack_top:

; ==================================================================================================
;
;                   [ ... Önceki ~1050 satır kod burada yer alıyor ... ]
;
; ==================================================================================================
;
; ##################################################################################################
; # BÖLÜM 8: CPU KİMLİK TESPİTİ (CPUID)
; ##################################################################################################
; Bu bölüm, CPUID komutunu kullanarak işlemcinin üreticisini, modelini ve desteklediği
; özellikleri tespit etmek için fonksiyonlar içerir. Bu, çekirdeğin, üzerinde çalıştığı
; donanıma adapte olabilmesi için kritiktir.

section .text
global cpuid_get_vendor_string
global cpuid_check_feature

; --- CPUID Vendor String Alıcı ---
; Açıklama:
;   CPUID komutunu EAX=0 ile çağırarak 12 byte'lık üretici kimlik dizesini alır.
;   ("GenuineIntel", "AuthenticAMD", vb.)
; Argümanlar:
;   [esp+4]: Sonucun yazılacağı 13 byte'lık tamponun adresi.
; Çıktı:
;   Tampon, null sonlandırılmış vendor string ile doldurulur.
cpuid_get_vendor_string:
    push ebp
    mov ebp, esp
    push ebx
    push ecx
    push edx

    mov eax, 0              ; EAX=0, vendor string için komut.
    cpuid                   ; CPUID komutunu çalıştır.

    ; CPUID, sonucu EBX, EDX, ECX sırasında döndürür.
    mov edi, [ebp + 8]      ; Argüman olarak verilen tampon adresini al.
    mov [edi], ebx
    mov [edi + 4], edx
    mov [edi + 8], ecx
    mov byte [edi + 12], 0  ; String'i null ile sonlandır.

    pop edx
    pop ecx
    pop ebx
    mov esp, ebp
    pop ebp
    ret


; --- CPUID Özellik Kontrolcüsü ---
; Açıklama:
;   Belirli bir CPUID özelliğinin desteklenip desteklenmediğini kontrol eder.
; Argümanlar:
;   [esp+4]: Özellik sorgusu için EAX'a yüklenecek değer.
;   [esp+8]: Kontrol edilecek register (0=ECX, 1=EDX).
;   [esp+12]: Kontrol edilecek bit.
; Çıktı:
;   EAX: 1 (destekleniyor), 0 (desteklenmiyor).
cpuid_check_feature:
    push ebp
    mov ebp, esp
    push ebx
    push ecx
    push edx

    mov eax, [ebp + 8]      ; Sorgu numarasını al.
    cpuid

    mov edi, [ebp + 12]     ; Hangi register'ın kontrol edileceğini al (0=ECX, 1=EDX).
    mov esi, [ebp + 16]     ; Hangi bit'in kontrol edileceğini al.

    cmp edi, 0
    je .check_ecx
    cmp edi, 1
    je .check_edx

.feature_not_found:
    mov eax, 0
    jmp .done

.check_ecx:
    bt ecx, esi             ; Belirtilen bit'i Carry Flag'e taşı.
    jmp .check_carry

.check_edx:
    bt edx, esi
    ; Düşerek .check_carry'ye devam et.

.check_carry:
    jc .feature_found       ; Carry Flag set edilmişse, özellik var demektir.
    jmp .feature_not_found

.feature_found:
    mov eax, 1

.done:
    pop edx
    pop ecx
    pop ebx
    mov esp, ebp
    pop ebp
    ret


; ##################################################################################################
; # BÖLÜM 9: BELLEK HARİTASI TESPİTİ (BIOS INT 0x15, EAX=0xE820)
; ##################################################################################################
; Modern sistemlerde bellek miktarını ve kullanılabilir aralıkları tespit etmenin
; standart yolu BIOS E820 interrupt'ını kullanmaktır. Bu fonksiyon, BIOS'tan
; bellek haritasını alır ve belirtilen bir adrese yazar.

section .data
align 4
e820_map_entry_size     equ 20  ; Her E820 girdisi 20 byte'tır.
e820_max_entries        equ 128 ; En fazla 128 girdi saklayacağız.

section .bss
align 4
; BIOS'tan alınan bellek haritasını saklamak için alan.
e820_map:
    resb e820_map_entry_size * e820_max_entries
e820_map_count:
    resd 1

section .text
global detect_memory

; --- Bellek Tespit Fonksiyonu ---
; Açıklama:
;   BIOS interrupt 0x15, EAX=0xE820'yi döngüsel olarak çağırarak tüm bellek
;   haritasını alır ve `e820_map` adresine yazar.
; Argümanlar:
;   Yok.
; Çıktı:
;   EAX: 0 (başarısız), 1 (başarılı).
;   `e820_map`: Bellek haritası ile doldurulur.
;   `e820_map_count`: Haritadaki girdi sayısı ile doldurulur.
detect_memory:
    pusha                   ; Tüm register'ları koru.

    mov edi, e820_map       ; Sonuçların yazılacağı adres.
    xor ebx, ebx            ; Döngünün başlangıcında EBX sıfır olmalı.
    xor ecx, ecx
    mov [e820_map_count], ecx ; Girdi sayacını sıfırla.

.loop:
    mov eax, 0xE820         ; E820 fonksiyonu.
    mov edx, 0x534D4150     ; 'SMAP' (ASCII) sihirli sayısı.
    mov ecx, 20             ; Girdi boyutu (20 byte).
    push edi                ; `int` komutu EDI'yi bozabilir, koru.
    int 0x15                ; BIOS interrupt'ını çağır.
    pop edi

    jc .error               ; Carry flag set ise hata var demektir.
    cmp eax, 0x534D4150     ; Sihirli sayı bozulmuş mu kontrol et.
    jne .error

    ; EBX=0 ise ve döngüye devam ediyorsak, bu son girdi demektir.
    ; Ancak bazı BIOS'lar EBX=0 olmasına rağmen CF'yi set etmez. Bu yüzden
    ; hem EBX'i hem de CF'yi kontrol etmek daha güvenlidir.
    test ebx, ebx           ; EBX sıfır mı?
    jz .finished

    ; Girdi başarılı, sayacı artır ve bir sonraki girdiye geç.
    add edi, 20             ; Bir sonraki girdi için pointer'ı ilerlet.
    inc dword [e820_map_count]
    cmp dword [e820_map_count], e820_max_entries
    je .finished            ; Maksimum girdi sayısına ulaştık.
    jmp .loop

.error:
    mov eax, 0              ; Başarısızlık.
    jmp .exit

.finished:
    mov eax, 1              ; Başarı.

.exit:
    popa
    ret


; ##################################################################################################
; # BÖLÜM 10: SAYFALAMA (PAGING) MEKANİZMASINI BAŞLATMA
; ##################################################################################################
; Sayfalama, sanal bellek yönetiminin temelidir. Bu bölüm, ilk sayfa dizinini (Page
; Directory) ve sayfa tablolarını (Page Tables) oluşturur, ardından sayfalama
; mekanizmasını etkinleştirir.
;
; Strateji: İlk 4 MB'lık fiziksel belleği, aynı sanal adreslere haritalayacağız
; (identity mapping). Bu, sayfalama etkinleştirildikten sonra çekirdeğin sorunsuz
; bir şekilde çalışmaya devam etmesini sağlar.

section .bss
align 4096                  ; Sayfa dizini ve tabloları 4K hizalı olmalıdır.
page_directory:
    resb 4096
first_page_table:
    resb 4096

section .text
global paging_install

; --- Paging Sabitleri ---
PAGE_FLAG_PRESENT       equ 1 << 0  ; Sayfa bellekte mevcut.
PAGE_FLAG_READWRITE     equ 1 << 1  ; Okuma/Yazma izni var.
PAGE_FLAG_USER          equ 1 << 2  ; Kullanıcı modu erişimi var (Ring 3).
PAGE_FLAG_WRITETHROUGH  equ 1 << 3  ;
PAGE_FLAG_CACHEDISABLE  equ 1 << 4  ;
PAGE_FLAG_ACCESSED      equ 1 << 5  ;
PAGE_FLAG_DIRTY         equ 1 << 6  ; (Sadece sayfa tablolarında)
PAGE_FLAG_4MB           equ 1 << 7  ; (Sadece sayfa dizininde 4MB'lık sayfalar için)

; --- Paging Kurulum Fonksiyonu ---
paging_install:
    ; 1. Sayfa dizinini ve ilk sayfa tablosunu sıfırlarla doldur.
    mov edi, page_directory
    mov ecx, 1024           ; 1024 girdi * 4 byte = 4096 byte
    xor eax, eax
    rep stosd               ; EDI'yi EAX (0) ile ECX kadar doldur.

    mov edi, first_page_table
    mov ecx, 1024
    xor eax, eax
    rep stosd

    ; 2. İlk 4 MB'ı haritalayacak olan sayfa tablosunu oluştur.
    ;    Her girdi bir 4 KB'lık sayfayı temsil eder. 1024 girdi * 4 KB = 4 MB.
    mov edi, first_page_table
    mov ecx, 1024           ; 1024 sayfa
    mov eax, PAGE_FLAG_PRESENT | PAGE_FLAG_READWRITE
.map_page_loop:
    mov [edi], eax
    add eax, 4096           ; Bir sonraki 4K'lık fiziksel sayfanın adresi.
    add edi, 4              ; Bir sonraki sayfa tablosu girdisi.
    loop .map_page_loop

    ; 3. Sayfa dizinini ayarla.
    ;    İlk girdiyi, oluşturduğumuz ilk sayfa tablosuna işaret edecek şekilde
    ;    ayarlıyoruz.
    mov eax, first_page_table
    or eax, PAGE_FLAG_PRESENT | PAGE_FLAG_READWRITE
    mov [page_directory], eax

    ; 4. Sayfalama mekanizmasını etkinleştir.
    ;    a. Sayfa dizininin fiziksel adresini CR3 register'ına yükle.
    mov eax, page_directory
    mov cr3, eax

    ;    b. CR0 register'ındaki PG (Paging) bitini (bit 31) set et.
    mov eax, cr0
    or eax, 0x80000000      ; PG bitini set et.
    mov cr0, eax

    ; 5. Komut önbelleğini temizle.
    ;    Sayfalama etkinleştirildikten sonra, işlemcinin adres çevirme
    ;    birimi (TLB) ve komut boru hattı eski (fiziksel) adresleri
    ;    içerebilir. Bir `jmp` komutu ile bunu temizliyoruz.
    jmp .paging_enabled
.paging_enabled:
    ret


; ##################################################################################################
; # BÖLÜM 11: PROGRAMLANABİLİR ZAMANLAYICI (PIT) KURULUMU
; ##################################################################################################
; PIT, periyodik kesmeler üreterek zamanlama (scheduling) gibi işlemler için
; bir "kalp atışı" sağlar.

PIT_COMMAND_PORT    equ 0x43
PIT_CHANNEL0_PORT   equ 0x40
PIT_BASE_FREQUENCY  equ 1193182   ; PIT'in temel frekansı (Hz)

section .text
global pit_install

; --- PIT Kurulum Fonksiyonu ---
; Açıklama:
;   PIT'i belirli bir frekansta kesme üretecek şekilde programlar.
; Argümanlar:
;   [esp+4]: İstenen frekans (Hz).
pit_install:
    push ebp
    mov ebp, esp

    mov eax, [ebp + 8]      ; İstenen frekansı al.
    cmp eax, 0
    je .exit                ; Frekans sıfır olamaz.

    ; Bölücüyü (divisor) hesapla.
    mov edx, 0
    mov ebx, PIT_BASE_FREQUENCY
    div ebx                 ; EAX = BASE / freq
    mov ecx, eax            ; Bölücüyü ECX'e kaydet.

    ; Komut byte'ını gönder: Kanal 0, LSB/MSB access, Kare Dalga modu (Mode 3).
    mov al, 0x36
    out PIT_COMMAND_PORT, al

    ; Bölücünün düşük byte'ını gönder.
    mov al, cl
    out PIT_CHANNEL0_PORT, al

    ; Bölücünün yüksek byte'ını gönder.
    mov al, ch
    out PIT_CHANNEL0_PORT, al

.exit:
    mov esp, ebp
    pop ebp
    ret


; ##################################################################################################
; # BÖLÜM 12: BASİT PS/2 KLAVYE GİRİŞİ
; ##################################################################################################
; Bu bölüm, klavye kontrolcüsünden doğrudan bir tarama kodu (scancode) okumak
; için temel fonksiyonlar içerir. Bu, kesme tabanlı bir sürücüden önce
; kullanılabilir.

KEYBOARD_DATA_PORT      equ 0x60
KEYBOARD_STATUS_PORT    equ 0x64

section .text
global keyboard_read_scancode

; --- Klavye Tarama Kodu Okuma Fonksiyonu ---
; Açıklama:
;   Klavye tamponu dolana kadar bekler ve gelen tarama kodunu okur.
;   Bu, engelleyici (blocking) bir fonksiyondur.
; Argümanlar:
;   Yok.
; Çıktı:
;   AL: Okunan tarama kodu.
keyboard_read_scancode:
.wait_for_buffer:
    in al, KEYBOARD_STATUS_PORT
    test al, 1              ; Çıktı tamponu dolu mu (bit 0)?
    jz .wait_for_buffer     ; Değilse, bekle.

    ; Tampon dolu, veriyi oku.
    in al, KEYBOARD_DATA_PORT
    ret

; ===============================================================================================
;
; ##################################################################################################
; # BÖLÜM 13: FPU (FLOATING POINT UNIT) / SSE KURULUMU
; ##################################################################################################
; Modern işlemciler, kayan noktalı sayılarla (floating-point) ve SIMD (Single Instruction,
; Multiple Data) işlemleriyle çalışmak için özel birimlere sahiptir. Bu birimleri
; kullanmadan önce, CR0 ve CR4 kontrol register'larında bazı bitleri ayarlayarak
; onları başlatmamız gerekir. Bu, özellikle çoklu görev (multitasking) sırasında
; FPU/SSE durumunu (context) doğru bir şekilde yönetmek için önemlidir.

section .text
global fpu_sse_install

fpu_sse_install:
    ; 1. FPU'nun varlığını kontrol et. (Pentium II'de zaten var ama bu iyi bir pratiktir)
    ;    Pentium'dan beri FPU standarttır, bu yüzden bu adımı atlıyoruz.

    ; 2. CR0'daki FPU ile ilgili bitleri ayarla.
    mov eax, cr0
    and eax, 0xFFFB      ; EM (Emulation) bitini (bit 2) temizle. Bu, FPU'nun
                         ; donanım tarafından yönetileceğini belirtir.
    or  eax, 0x20        ; NE (Numeric Error) bitini (bit 5) set et. Bu,
                         ; FPU hatalarının modern şekilde (interrupt 16)
                         ; raporlanmasını sağlar.
    mov cr0, eax

    ; 3. CR4'teki SSE ile ilgili bitleri ayarla.
    ;    a. SSE'nin desteklenip desteklenmediğini CPUID ile kontrol et.
    mov eax, 1           ; EAX=1, özellikler için komut.
    cpuid
    bt edx, 25           ; EDX'in 25. biti SSE desteğini gösterir.
    jnc .no_sse          ; Carry yoksa, SSE desteği yok demektir.

    ;    b. SSE desteği var, OSFXSR (bit 9) ve OSXMMEXCPT (bit 10) bitlerini set et.
    mov eax, cr4
    or eax, 0x00000600   ; OSFXSR ve OSXMMEXCPT bitlerini set et.
                         ; OSFXSR: FXSAVE/FXRSTOR komutlarını etkinleştirir.
                         ; OSXMMEXCPT: SIMD kayan nokta istisnalarını etkinleştirir.
    mov cr4, eax
    
    ; 4. FPU/SSE birimini başlat (init).
    ;    Bu, FPU kontrol kelimesini varsayılan değerlere ayarlar ve
    ;    tüm FPU/MMX/XMM register'larını temizler.
    finit                ; FPU'yu başlat.
    
    ; SSE'yi destekliyorsak, XMM register'larını da temizleyelim.
    ; Bunun için bellekte 16-byte hizalı sıfır bir alan gerekir.
    ; xorps xmm0, xmm0 gibi komutlar da kullanılabilir.
    jmp .fpu_done

.no_sse:
    ; SSE desteği yok, sadece FPU'yu başlat.
    finit

.fpu_done:
    ret


; ##################################################################################################
; # BÖLÜM 14: VGA METİN MODU KONTROLÜ
; ##################################################################################################
; Bu bölüm, çekirdeğin ilk aşamalarında ekrana yazı yazmak için kullanılacak temel
; VGA metin modu fonksiyonlarını içerir. Bu fonksiyonlar, C kütüphaneleri mevcut
; olmadan önce hata ayıklama (debugging) için hayati önem taşır.

VGA_MEMORY_ADDRESS  equ 0xB8000
VGA_WIDTH           equ 80
VGA_HEIGHT          equ 25

section .data
align 4
vga_cursor_x        dd 0
vga_cursor_y        dd 0
vga_text_attribute  db 0x0F     ; Beyaz üzerine siyah varsayılan renk.

section .text
global vga_clear_screen
global vga_print_char
global vga_print_string
global vga_set_cursor
global vga_scroll_screen

; --- Ekranı Temizleme Fonksiyonu ---
; Açıklama:
;   Tüm ekranı boş karakterlerle ve varsayılan renkle doldurur.
vga_clear_screen:
    pusha
    mov edi, VGA_MEMORY_ADDRESS
    mov ecx, VGA_WIDTH * VGA_HEIGHT
    mov al, ' '                 ; Boş karakter
    mov ah, [vga_text_attribute]
    rep stosw                   ; Tüm video belleğini (AL, AH) ile doldur.
    
    ; İmleci (cursor) başa al.
    mov dword [vga_cursor_x], 0
    mov dword [vga_cursor_y], 0
    call vga_update_hw_cursor

    popa
    ret

; --- Tek Karakter Yazdırma Fonksiyonu ---
; Açıklama:
;   Mevcut imleç pozisyonuna bir karakter yazar ve imleci ilerletir.
;   Satır sonu ('\n'), geri silme ('\b') gibi özel karakterleri yönetir.
; Argümanlar:
;   AL: Yazdırılacak karakter.
vga_print_char:
    pusha
    movzx ecx, al               ; Karakteri ECX'e al.

    cmp cl, 0x08                ; Backspace mi?
    je .handle_backspace
    cmp cl, '\n'                ; Newline (satır sonu) mu?
    je .handle_newline

    ; Normal karakter yazdırma
.print:
    mov edi, [vga_cursor_y]
    mov eax, VGA_WIDTH
    mul edi
    add eax, [vga_cursor_x]
    shl eax, 1                  ; 2 ile çarp (her karakter 2 byte)
    add eax, VGA_MEMORY_ADDRESS

    mov ah, [vga_text_attribute]
    mov al, cl
    mov [eax], ax

    ; İmleci ilerlet.
    inc dword [vga_cursor_x]
    mov edx, [vga_cursor_x]
    cmp edx, VGA_WIDTH
    jl .update_cursor_and_exit  ; Satır sonuna gelmediysek çık.

.handle_newline:
    mov dword [vga_cursor_x], 0
    inc dword [vga_cursor_y]
    jmp .check_scroll

.handle_backspace:
    mov edx, [vga_cursor_x]
    cmp edx, 0
    je .update_cursor_and_exit  ; Satır başında isek bir şey yapma.
    dec dword [vga_cursor_x]
    
    ; Backspace yapılan karakteri boşluk ile değiştir.
    mov edi, [vga_cursor_y]
    mov eax, VGA_WIDTH
    mul edi
    add eax, [vga_cursor_x]
    shl eax, 1
    add eax, VGA_MEMORY_ADDRESS
    mov ah, [vga_text_attribute]
    mov al, ' '
    mov [eax], ax
    jmp .update_cursor_and_exit

.check_scroll:
    mov edx, [vga_cursor_y]
    cmp edx, VGA_HEIGHT
    jl .update_cursor_and_exit  ; Ekran sonuna gelmediysek çık.

    call vga_scroll_screen
    mov dword [vga_cursor_y], VGA_HEIGHT - 1

.update_cursor_and_exit:
    call vga_update_hw_cursor
    popa
    ret


; --- String Yazdırma Fonksiyonu ---
; Açıklama:
;   Null sonlandırılmış bir string'i ekrana yazar.
; Argümanlar:
;   ESI: Yazdırılacak string'in adresi.
vga_print_string:
    pusha
.loop:
    lodsb                       ; ESI'den bir byte AL'ye yükle ve ESI'yi artır.
    test al, al                 ; Null karakter mi?
    jz .done
    call vga_print_char         ; Karakteri yazdır.
    jmp .loop
.done:
    popa
    ret


; --- Ekranı Kaydırma Fonksiyonu ---
vga_scroll_screen:
    pusha
    ; 1. Her satırı bir üst satıra kopyala.
    mov esi, VGA_MEMORY_ADDRESS + (VGA_WIDTH * 2) ; 1. satırın başı
    mov edi, VGA_MEMORY_ADDRESS                   ; 0. satırın başı
    mov ecx, (VGA_WIDTH * (VGA_HEIGHT - 1))
    rep movsw                                     ; Kelime kelime kopyala.

    ; 2. Son satırı boşluklarla temizle.
    mov edi, VGA_MEMORY_ADDRESS + (VGA_WIDTH * (VGA_HEIGHT - 1) * 2)
    mov ecx, VGA_WIDTH
    mov al, ' '
    mov ah, [vga_text_attribute]
    rep stosw

    popa
    ret


; --- Donanım İmlecini Güncelleme Fonksiyonu ---
vga_update_hw_cursor:
    pusha
    ; İmleç pozisyonunu hesapla: pos = y * 80 + x
    mov edi, [vga_cursor_y]
    mov eax, VGA_WIDTH
    mul edi
    add eax, [vga_cursor_x]

    ; Düşük byte'ı gönder
    mov dx, 0x3D4
    mov al, 0x0F
    out dx, al
    mov dx, 0x3D5
    mov al, eax
    out dx, al

    ; Yüksek byte'ı gönder
    mov dx, 0x3D4
    mov al, 0x0E
    out dx, al
    mov dx, 0x3D5
    shr eax, 8
    mov al, eax
    out dx, al
    popa
    ret


; ##################################################################################################
; # BÖLÜM 15: GÖREV DURUM SEGMENTİ (TSS) KURULUMU
; ##################################################################################################
; TSS, donanım tabanlı çoklu görev (hardware multitasking) ve daha önemlisi,
; Ring 3'ten Ring 0'a geçişlerde (sistem çağrıları, kesmeler) işlemcinin
; kernel yığınını (kernel stack) nerede bulacağını bilmesi için kullanılır.
; Her CPU için bir TSS tanımlanmalıdır.

section .data
align 4
; GDT'ye eklenecek olan TSS segment tanımlayıcısı.
gdt_tss:
    dw 0                    ; limit (low)
    dw 0                    ; base (low)
    db 0                    ; base (middle)
    db 0x89                 ; type=9 (32-bit TSS), dpl=0, p=1
    db 0                    ; limit (high) + flags
    db 0                    ; base (high)

section .bss
align 16
; TSS yapısının kendisi. 104 byte.
tss_entry:
    resb 104

section .text
global tss_install

tss_install:
    ; 1. TSS için GDT girdisini ayarla.
    mov eax, tss_entry          ; TSS yapısının adresi
    mov ecx, 104                ; TSS boyutu

    mov [gdt_tss + 2], ax       ; Base (0-15)
    shr eax, 16
    mov [gdt_tss + 4], al       ; Base (16-23)
    mov [gdt_tss + 7], ah       ; Base (24-31)

    mov [gdt_tss], cx           ; Limit (0-15)

    ; Flags & Limit (16-19)
    ; Limit 104, yani 0x68. Bu, en üst 4 biti 0 yapar.
    ; Diğer bayraklar (granularity, size, vs.) TSS için 0'dır.
    mov byte [gdt_tss + 6], 0x00


    ; 2. TSS yapısını doldur.
    ;    En önemli alanlar SS0 ve ESP0'dır. Bunlar, bir kullanıcı modu süreci
    ;    bir kesme veya sistem çağrısı yaptığında CPU'nun hangi kernel
    ;    yığınını (stack) kullanacağını belirtir.
    mov edi, tss_entry
    mov ecx, 104 / 4
    xor eax, eax
    rep stosd                   ; TSS'i sıfırla.

    mov word [tss_entry + 4], 0x10  ; SS0: Kernel Veri Segmenti
    ; ESP0, her görev değiştiğinde o görevin kernel yığınına ayarlanmalıdır.
    ; Şimdilik başlangıç yığınımızı koyuyoruz.
    mov dword [tss_entry + 8], kernel_stack_top

    ; 3. TSS'i yükle.
    ;    GDT'deki TSS segmentinin ofsetini (index * 8) TR (Task Register)
    ;    register'ına `ltr` komutu ile yüklüyoruz.
    ;    gdt_tss, gdt_data'dan sonra gelir. GDT offsetleri:
    ;    NULL=0, CODE=8, DATA=16, TSS=24 -> 0x18
    mov ax, 0x18
    ltr ax

    ret

; ##################################################################################################
; # BÖLÜM 16: GELİŞMİŞ _START RUTİNİ VE KERNEL'E GEÇİŞ
; ##################################################################################################
; Bu bölüm, önceki _start rutinini, yeni eklenen tüm kurulum fonksiyonlarını
; çağıracak şekilde genişletir. Bu, C çekirdeğine geçmeden önce tüm donanımın
; ve CPU'nun temel seviyede hazır olmasını garantiler.

section .text
; _start etiketini yeniden tanımlayarak önceki basit versiyonun üzerine yazıyoruz.
_start:
    cli                         ; Her şeyden önce kesmeleri kapat.
    mov esp, kernel_stack_top   ; Kendi yığınımızı kur.

    ; --- Donanım ve Ortam Başlatma Sırası ---

    ; 1. Ekranı temizle ve bir "boot" mesajı yaz. Bu, sistemin nerede
    ;    kilitlendiğini anlamak için çok faydalıdır.
    call vga_clear_screen
    mov esi, boot_message
    call vga_print_string

    ; 2. GDT ve TSS'i kur. TSS, GDT'ye bir girdi eklediği için
    ;    GDT kurulmadan önce ayarlanmalıdır.
    call gdt_install
    call tss_install
    mov esi, gdt_ok_message
    call vga_print_string

    ; 3. IDT ve PIC'i kur.
    call idt_install
    mov esi, idt_ok_message
    call vga_print_string

    ; 4. PIT'i (Zamanlayıcı) kur. (örn: 100 Hz)
    push 100
    call pit_install
    add esp, 4
    mov esi, pit_ok_message
    call vga_print_string

    ; 5. FPU/SSE'yi başlat.
    call fpu_sse_install
    mov esi, fpu_ok_message
    call vga_print_string

    ; 6. Sayfalamayı (Paging) etkinleştir.
    call paging_install
    mov esi, paging_ok_message
    call vga_print_string

    ; --- Yüksek Seviyeli Çekirdeğe Kontrolü Devret ---
    mov esi, handover_message
    call vga_print_string

    sti                         ; Kesmeleri artık güvenle açabiliriz.
    
    push ebx                    ; Multiboot info struct
    call kmain                  ; C çekirdeğini çağır.

    ; --- Güvenlik Döngüsü ---
    cli
.final_hang:
    hlt
    jmp .final_hang


section .data
align 4
boot_message        db 'Imperium OS Booting...', 0x0A, 0
gdt_ok_message      db ' [ OK ] GDT and TSS installed.', 0x0A, 0
idt_ok_message      db ' [ OK ] IDT and PIC configured.', 0x0A, 0
pit_ok_message      db ' [ OK ] PIT initialized.', 0x0A, 0
fpu_ok_message      db ' [ OK ] FPU/SSE units enabled.', 0x0A, 0
paging_ok_message   db ' [ OK ] Paging mechanism enabled.', 0x0A, 0
handover_message    db 0x0A, 'Handing over control to high-level kernel...', 0x0A, 0x0A, 0

