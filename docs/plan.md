# xp-trim — Uygulama planı

## Bağlam
Windows XP'de yerleşik TRIM desteği yok, bu yüzden XP'de SSD kullananlar zamanla yazma
performansı kaybediyor. Hedef: XP SP0–SP3 (x86) üzerinde NTFS birimlerindeki boş alanı
güvenli şekilde TRIM'leyen dört parçalı bir paket:

| Parça | Dosya | Görev |
|---|---|---|
| Komut satırı aracı | `xptrim.exe` | info / trim (dry-run varsayılan) / kurulum |
| Servis | `xptrimsvc.exe` | Periyodik çalıştırma, durum dosyası yazma |
| Denetim Masası appleti | `xptrim.cpl` | Durumu görme, ayarları değiştirme, şimdi çalıştırma |
| Tepsi simgesi (isteğe bağlı) | `xptrimtray.exe` | Saatin yanında durum; çift tık → applet |

Grafik parçalar XP'nin kendi kontrolleriyle (property sheet, ListView, comctl32 v6
manifesti) yapılır; görünüm XP'nin Luna ve klasik temalarıyla birebir uyumlu olur.
MIT lisansıyla GitHub'da yayınlanacak.

Depoda şu an yalnızca kurallar var (CLAUDE.md, .gitignore, .gitattributes, CHANGELOG.md).

**Verilen kararlar:** MIT lisansı · kod/yorum/commit İngilizce, README TR+EN · gerçek XP testi
ayrı bir x86 makinede/VM'de kullanıcı tarafından · dry-run, apply ile aynı yolu izler
(yer tutucu dosya oluşturur ve siler, ham diske yazmaz).

**Ortam:** Geliştirme makinesi Windows 11 **ARM64**. i686 exe'ler burada x86 emülasyonuyla
çalışır, yani birim testleri ana makinede koşar. Derleyici henüz kurulu değil.

## Temel tasarım: boş alan nasıl TRIM'lenir
1. Birim doğrulanır: NTFS, tek disk extent'i, bölüm tipi 0x42 değil, MBR disk.
2. Disk doğrulanır: ATA IDENTIFY ile word 169 bit 0 (TRIM desteği), word 105 (komut başına
   en fazla 512 baytlık blok). Okunamazsa çıkılır.
3. Kökte gizli bir yer tutucu dosya (`\xptrim.$$$`) açılır, `SetEndOfFile` ile
   boş alan − güvenlik payı (varsayılan max(%5, 256 MB)) kadar büyütülür. NTFS kümeleri
   ayırır ama veri yazılmaz (valid data length 0), bu yüzden hızlıdır. `FlushFileBuffers`.
4. `FSCTL_GET_RETRIEVAL_POINTERS` ile dosyanın extent'leri (LCN) alınır.
5. Her extent LBA'ya çevrilir (bölüm ofseti `IOCTL_DISK_GET_PARTITION_INFO`, sektör boyutu
   `IOCTL_DISK_GET_DRIVE_GEOMETRY`), bölüm sınırı kontrol edilir, aralıklar ATA DSM
   girdilerine (48-bit LBA + 16-bit uzunluk, ≤65535 sektör) bölünür.
6. Dry-run: aralıklar yazdırılır. Apply: DATA SET MANAGEMENT (0x06, feature 0x01) komutları
   gruplar halinde gönderilir. Pass-through yolu çalışma anında seçilir: önce
   `IOCTL_ATA_PASS_THROUGH`, olmazsa `IOCTL_SCSI_PASS_THROUGH_DIRECT` + SAT ATA-16 CDB.
7. Dosya her durumda silinir (hata, Ctrl+C, servis durdurma dahil).

Birimi kilitlemek gerekmez (C: için zaten mümkün değil) ve yarış koşulu ortadan kalkar:
TRIM'lenen kümeler o anda yalnızca bizim dosyamıza aittir.

## Bileşenler arası veri akışı
- **Yapılandırma:** `%ProgramFiles%\xp-trim\xptrim.ini` (`GetPrivateProfileStringW`):
  sürücüler, aralık (varsayılan 7 gün), `apply=0` varsayılan, güvenlik payı.
  Yalnızca yöneticiler yazabilir.
- **Durum:** servis her çalışmadan sonra
  `%ALLUSERSPROFILE%\Application Data\xp-trim\status.ini` yazar: diskler (model, boyut,
  TRIM desteği), birim başına son çalışma zamanı, mod, sonuç, TRIM'lenen bayt.
  Users okuyabilir, yalnızca servis yazabilir. Applet ve tepsi IDENTIFY gibi yönetici
  gerektiren sorguları kendileri yapmaz; bu dosyadan okur.
- **Komut:** "Şimdi çalıştır" servise özel kontrol kodu (128) ile iletilir
  (`ControlService`). Servis bunu yapılandırmadaki moda göre çalıştırır.
- **Log:** exe yanındaki döndürülen metin dosyası (Event Log mesaj kaynağı gerektirdiği için
  şimdilik değil).

## Denetim Masası appleti (`xptrim.cpl`)
- `CPlApplet` export eden bir DLL. Kayıt: `HKLM\...\Control Panel\Cpls` değeri;
  kategori "Performans ve Bakım" (`Extended Properties` anahtarı; kategori kimliği
  uygulamada MSDN'den doğrulanır).
- `control.exe` içinde barındırıldığı için XP görsel stili DLL'de ayrıca etkinleştirilmelidir:
  manifest kaynak kimliği 2 + `ISOLATION_AWARE_ENABLED` (aksi halde klasik görünür).
- Property sheet sekmeleri:
  - **Genel:** servis durumu (çalışıyor/durdu, başlat/durdur), son çalışma ve sonucu,
    "Şimdi çalıştır", ☐ **Saatin yanında simge göster** (tepsiyi HKCU `Run` anahtarına
    ekler/kaldırır ve hemen başlatır/kapatır).
  - **Diskler:** report görünümlü ListView: Sürücü · Disk modeli · Boyut · TRIM desteği ·
    Son TRIM · Sonuç. Desteklenmeyen/reddedilen birimler gerekçesiyle gösterilir.
  - **Zamanlama:** hangi sürücüler, aralık, güvenlik payı, ☐ **Gerçek TRIM'i etkinleştir**
    (varsayılan kapalı; işaretlenince ne yapacağını anlatan bir onay penceresi çıkar).
  - **Günlük:** log dosyasının son satırları, salt-okunur.
- Yönetici olmayan kullanıcıda ayar değiştiren kontroller devre dışıdır ve nedeni yazar.
- Metinler string table'da (EN + TR), dil `GetUserDefaultUILanguage` ile seçilir.

## Tepsi simgesi (`xptrimtray.exe`, isteğe bağlı)
- Yalnızca applette kutu işaretlenince çalışır; varsayılan kapalı.
- `Shell_NotifyIconW` (`NOTIFYICONDATA_V2_SIZE`, `NOTIFYICON_VERSION`). Üç ikon durumu:
  hazır, çalışıyor, uyarı/hata. Durum değişince balon bildirim. `status.ini`'yi
  `FindFirstChangeNotificationW` ile izler. Explorer yeniden başlarsa `TaskbarCreated`
  ile simge geri gelir.
- Araç ipucunda özet (ör. "C: son TRIM 3 gün önce, 12,4 GB").
- **Çift tık → applet açılır** (`control.exe xptrim.cpl`). Sağ tık: Aç · Şimdi çalıştır ·
  Simgeyi gizle · Çıkış. Ayar değiştirmez, apply'ı açamaz.
- İkonlar projeye özgü, sıfırdan çizilir (16/32 px, XP'nin 32-bit alfa ikon biçimi);
  applet, tepsi ve exe'ler aynı ikon ailesini paylaşır.

## Kod yapısı
```
Makefile                 # MSYS2 MINGW32; tek komutla build / test / check
src/core/                # saf C, Win32 yok, birim testli: geometry.c (LCN→LBA, sınır),
                         # ranges.c, dsm.c, identify.c, status.c (status.ini modeli), fmt.c
src/win32/               # volume.c, passthru.c, device_real.c, placeholder.c, log.c,
                         # config.c, svcctl.c (servis kontrolü — applet/tepsi/CLI ortak)
src/device.h             # device arayüzü (identify, trim) — gerçek ve sahte uygulama
src/cli/main.c           # xptrim: info | trim [--apply] <X:> | install | uninstall
src/service/svc.c        # xptrimsvc
src/cpl/                 # cpl.c (CPlApplet), page_*.c, cpl.rc, cpl.def, manifest
src/tray/                # tray.c, tray.rc
res/                     # ikonlar, ortak string'ler
tests/                   # küçük kendi test çatısı + mock_device.c
tools/check-imports.sh   # objdump -p → tools/allowed-imports.txt ile karşılaştırma
```
- `allowed-imports.txt`: kullanılan her DLL!fonksiyon, MSDN'de "Windows XP" olarak
  doğrulandığı notuyla. Listede olmayan import derlemeyi düşürür (exe'ler ve .cpl için).
- `xptrim install`: dosyaları kopyalar, servisi kurar, appleti kaydeder. Tepsi kurulmaz,
  applet üzerinden açılır. `uninstall` hepsini geri alır.

## İş sırası (her biri ayrı branch + PR)

Durum: 0 ✅ (#1) · 1 ✅ (#2) · 2 🔄 (`feat/volume-info`) · 3–9 ⏳

0. **Altyapı** — `chore/bootstrap`
   - MSYS2'yi winget ile kur (onay alınarak): `mingw-w64-i686-gcc`, `make`, `binutils`.
     MINGW32 paketleri yoksa yedek: llvm-mingw'nin msvcrt/i686 hedefli sürümü.
   - Makefile, dizin iskeleti, sürümünü yazdıran `xptrim.exe`, import kontrolü, `LICENSE`
     (MIT), README.md (EN) + README.tr.md, CLAUDE.md güncellemesi (açık kararlar kapanır,
     dil kuralı, ARM64 notu, dört bileşen), CHANGELOG girdisi.
   - GitHub deposu oluşturma ve ilk push: kullanıcıya sorularak. PR akışı buradan başlar.
   - GitHub Actions: windows-latest + msys2/setup-msys2 (MINGW32) → build, test, check.
1. **Saf mantık** — `feat/core`: geometry, ranges, dsm, identify + testler (sınır taşması,
   4K sektör, 65535 bölme, birleştirme, word 105 sınırı, bozuk IDENTIFY).
   - Komut başına DSM bloğu, sürücü daha fazlasını bildirse de en fazla 8'dir
     (`XT_DSM_BLOCKS_CAP`, 512 girdi); word 105 = 0 ise 1 blok kullanılır.
   - Tek bir dosyanın extent'lerinden gelen aralıklar çakışamaz; çakışma hata sayılır.
2. **Birim/disk sorguları** — `feat/volume-info`: salt-okunur IOCTL'ler, reddetme kuralları,
   `xptrim info X:`.
   - Karar mantığı `src/core/volcheck.c`'de (birim testli); `src/win32/volume.c` yalnızca
     bilgi toplar. Ek çapraz kontroller: bölüm bilgisi ile disk extent'i aynı olmalı,
     disk ve NTFS sektör boyutları eşit olmalı, NTFS bölümden büyük olmamalı, tip baytında
     0x80 (NT FT set) varsa reddedilir.
3. **IDENTIFY** — `feat/ata-identify`: iki pass-through yolu, `info`'ya TRIM desteği/model.
4. **Dry-run** — `feat/dry-run`: yer tutucu dosya, retrieval pointers, aralık çıktısı,
   her yolda temizlik, Ctrl+C. Sahte cihaz testleri.
5. **Apply** — `feat/apply`: DSM gönderimi, gruplama, `--apply` kapısı, ilerleme, hatada
   durma. Ana makinede **asla** apply çalıştırılmaz; yalnızca mock testi.
6. **Servis + kurulum** — `feat/service`: SCM döngüsü, zamanlayıcı, INI, log,
   `status.ini`, kontrol kodu 128, `xptrim install/uninstall`.
7. **Denetim Masası appleti** — `feat/cpl`: dört sekme, kayıt, yönetici kontrolü, EN/TR,
   ikon ailesi.
8. **Tepsi simgesi** — `feat/tray`: simge, menü, balon, çift tıkla applet,
   applet onay kutusuyla bağlantı.
9. **Yayın** — `docs/release`: README'ler (ekran görüntüleri, uyarılar, desteklenmeyen
   yapılandırmalar), `v0.1.0` etiketi, release zip + SHA-256.

Adımlar sıralıdır; her biri öncekinin koduna dayanır. Paralel oturuma bölünecek bağımsız iş yok.

## Doğrulama
- Her PR'da: `make` (−Werror), `make test` (ana makinede x86 emülasyonuyla), `make check`
  (import beyaz listesi + `objdump -p` ile subsystem/OS version 5.01). CI aynısını koşar.
- Adım 2–3: `xptrim info C:` ana makinede yönetici olarak, yalnızca okuma. (Ana makinenin
  diski GPT ise reddetme yolunun çalıştığı görülür; bu beklenen sonuçtur.)
- Adım 4: dry-run ana makinede (dosya oluşturup siler, ham yazma yok).
- Adım 7–8: applet ve tepsi ana makinede örnek bir `status.ini` ile açılır, sekmeler ve
  menü ekran görüntüsüyle kontrol edilir. Servis kurulumu ana makinede yapılmaz.
  XP Luna/klasik görünümü yalnızca XP makinesinde doğrulanabilir.
- XP doğrulaması (kullanıcı, x86 makine): her sürüm öncesi SP0 ve SP3'te `info`, dry-run,
  servis, applet, tepsi; `--apply` yalnızca gözden çıkarılabilir bir SSD'de. Doğrulanmayan
  her şey PR'da "XP'de doğrulanmadı" diye işaretlenir.

## İlk adımda dokunulacak dosyalar
`Makefile`, `src/cli/main.c`, `tools/check-imports.sh`, `tools/allowed-imports.txt`,
`LICENSE`, `README.md`, `README.tr.md`, `CLAUDE.md`, `CHANGELOG.md`, `.github/workflows/ci.yml`.
