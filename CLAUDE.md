# CLAUDE.md

Windows XP üzerinde SSD'lere TRIM gönderen bir komut satırı aracı (`xptrim`) ve onu
periyodik çalıştıran bir Windows servisi. GitHub'da açık kaynak olarak yayınlanacak.

Bu dosyadaki kurallar her değişiklikte geçerlidir. "Güvenlik kuralları" bölümü
hiçbir gerekçeyle esnetilemez; bir kural bir işi engelliyorsa kural çiğnenmez,
durum kullanıcıya sorulur.

## Hedef platform

- Windows XP SP0, SP1, SP2 ve SP3, x86 (32-bit). XP'de olmayan hiçbir API kullanılmaz.
  - Yeni bir API eklemeden önce MSDN'deki "Minimum supported client" değerine bakılır.
    Değer "Windows XP" değilse (ör. Vista, XP SP2, Server 2003) kullanılmaz ya da
    `GetProcAddress` ile çalışma anında aranır ve yokluğunda düzgün bir yedek yol izlenir.
  - XP'de olmayan, sık karşılaşılan tuzaklar: `GetTickCount64`, SRW lock / condition
    variable, `InitializeCriticalSectionEx`, `CreateFile2`, `GetFileInformationByHandleEx`,
    `CancelIoEx`, `SERVICE_CONFIG_DELAYED_AUTO_START_INFO`, `SERVICE_CONTROL_PRESHUTDOWN`,
    `*_s` güvenli CRT fonksiyonları.
- Dil: C (C99), yalnızca Win32 API. C++ runtime, .NET, UCRT yok.
  - Tüm Win32 çağrıları Unicode (`W`) sürümleriyle yapılır; giriş noktası `wmain` (`-municode`).
  - XP'nin `msvcrt.dll`'i `%lld`, `%zu` gibi C99 biçimlerini desteklemez. Ya
    `__USE_MINGW_ANSI_STDIO=1` kullanılır ya da `%I64d` / `%lu` ile yazılır; karışık kullanım yok.
- Derleme: MinGW-w64 i686 (msvcrt), `-D_WIN32_WINNT=0x0501 -DWINVER=0x0501`, statik link,
  PE subsystem version 5.01.
  - Bağlayıcı bayrakları: `-static -static-libgcc`,
    `-Wl,--major-subsystem-version,5,--minor-subsystem-version,1,--major-os-version,5,--minor-os-version,1`.
  - winpthreads ve C++ bağımlılığı çeken hiçbir şey kullanılmaz.
  - Uyarılar hata sayılır: `-std=c99 -Wall -Wextra -Werror`.
  - Yeni MinGW-w64 CRT sürümleri XP'de olmayan fonksiyonları içeri çekebilir. Bu yüzden her
    derlemeden sonra çıktının import tablosu (`objdump -p`) XP SP0'da bulunan export'ların
    beyaz listesiyle karşılaştırılır; listede olmayan bir import derlemeyi başarısız sayar.

## Güvenlik kuralları (ihlal edilemez)

- Varsayılan mod DRY-RUN'dır. Gerçek TRIM yalnızca `--apply` parametresiyle yapılır.
  - Servis de aynı kurala uyar: yapılandırmada açıkça `apply` etkinleştirilmedikçe yalnızca
    dry-run çalışır ve ne yapacağını loglar.
- LBA hesabında bölüm ofseti her zaman `IOCTL_DISK_GET_PARTITION_INFO` ile okunur, varsayılmaz.
  - Mantıksal sektör boyutu da varsayılmaz (`IOCTL_DISK_GET_DRIVE_GEOMETRY`); küme boyutu
    `FSCTL_GET_NTFS_VOLUME_DATA` ile okunur.
  - Formül tek bir fonksiyonda yaşar ve birim testleriyle korunur:
    `LBA = (bölüm_başlangıç_bayt / sektör_bayt) + LCN * (küme_bayt / sektör_bayt)`.
  - Hesaplanan her aralık, gönderilmeden önce bölüm sınırları içinde mi diye doğrulanır;
    dışına taşan tek bir aralık bile tüm işlemi iptal eder.
- Dinamik disk, RAID ve NTFS dışı birimler reddedilir.
  - Birim tam olarak bir disk extent'ine eşlenmelidir (`IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS`);
    birden fazla extent (spanned/striped) reddedilir.
  - Bölüm tipi 0x42 (LDM / dinamik disk), GPT diskler ve dosya sistemi adı tam olarak
    `NTFS` olmayan birimler reddedilir.
- Cihaz gerçekten TRIM destekliyorsa TRIM gönderilir: ATA IDENTIFY DEVICE word 169 bit 0
  kontrol edilir; tek komuttaki aralık sayısı word 105'e göre sınırlanır. Bilgi okunamıyorsa
  (USB köprüsü, bilinmeyen sürücü vb.) işlem yapılmaz.
- Boş alan yarışına karşı koruma: bağlı ve kilitlenmemiş bir birimde, bitmap anlık
  görüntüsünden alınan "boş" kümeler doğrudan TRIM'lenmez (okuma ile TRIM arasında dosya
  sistemi o kümeleri tahsis edebilir). Ya birim `FSCTL_LOCK_VOLUME` ile kilitlenir ya da boş
  alan önce araca ait bir yer tutucu dosyaya tahsis edilir ve yalnızca o dosyanın extent'leri
  (`FSCTL_GET_RETRIEVAL_POINTERS`) TRIM'lenir.
- ATA pass-through yolu (`IOCTL_ATA_PASS_THROUGH` ya da SAT CDB ile
  `IOCTL_SCSI_PASS_THROUGH_DIRECT`) SP sürümüne göre varsayılmaz; çalışma anında denenir,
  hiçbiri çalışmıyorsa işlem yapılmadan çıkılır.
- Testlerde veya örneklerde gerçek bir fiziksel diske asla yazılmaz.
  - Cihaza komut gönderen kod tek bir soyutlama katmanının (`device` arayüzü) arkasındadır;
    testler sahte (mock) cihaz kullanır ve gönderilen aralıkları kaydedip doğrular.
  - Dokümantasyondaki ve README'deki örnek komutlar `--apply` içermez.
  - Claude bu makinede `--apply` ile ya da `\\.\PhysicalDriveN` / `\\.\X:` üzerinde yazma
    yetkisiyle hiçbir şey çalıştırmaz. Gerçek donanımda doğrulama yalnızca kullanıcı
    tarafından, gözden çıkarılabilir bir diskte ve elle yapılır.

## Mimari (planlanan)

- `src/core/`   — saf mantık: LCN→LBA dönüşümü, aralık birleştirme/bölme, ATA DSM yükü
                  paketleme. Win32 bağımlılığı yok; ana makinede birim testine açık.
- `src/win32/`  — birim/disk sorguları, pass-through, `device` arayüzünün gerçek uygulaması.
- `src/cli/`    — `xptrim.exe` komut satırı aracı.
- `src/service/`— servis (`xptrimsvc.exe`): kurulum/kaldırma, zamanlayıcı, loglama.
- `tests/`      — birim testleri ve sahte cihaz.
- Servis LocalSystem olarak çalışır, `SERVICE_CONTROL_STOP`'a TRIM grupları arasında
  birkaç saniye içinde yanıt verir ve işlemi yarıda bırakmak güvenlidir.
- Çıkış kodları ve log biçimi belgelenir; dry-run çıktısı, apply modunda gönderilecek
  aralıklarla birebir aynıdır.

## Çalışma düzeni

- Her iş ayrı branch'te yapılır ve PR ile birleştirilir. `main`'e doğrudan commit atılmaz
  (ilk kurulum commit'i hariç). Branch adları commit tipini izler: `feat/...`, `fix/...`,
  `docs/...`, `chore/...`.
- Commit mesajları Conventional Commits formatında (`feat:`, `fix:`, `docs:`, `test:`,
  `refactor:`, `build:`, `ci:`, `chore:`). Geriye dönük uyumsuz değişiklikte `!` ve
  `BREAKING CHANGE:` kullanılır.
- Her değişiklikten sonra derleme, import tablosu kontrolü ve testler çalıştırılır.
  Başarısız olan bir adım varsa commit atılmaz; çıktı olduğu gibi raporlanır.
- Kullanıcıya görünen her değişiklik `CHANGELOG.md`'nin "Unreleased" bölümüne yazılır
  (Keep a Changelog başlıkları: Added, Changed, Fixed, Removed, Security).
- Ana makine (Windows 11) yalnızca derleme ve birim testi içindir. "XP'de çalışıyor"
  iddiası ancak gerçek bir XP (tercihen SP0 ve SP3) sanal makinesinde denendikten sonra
  yapılır; denenmemişse açıkça "XP'de doğrulanmadı" denir.
- Yayın: sürüm etiketleri `vX.Y.Z`; release ekinde exe'ler ve SHA-256 özetleri bulunur.

## Açık kararlar

- Lisans (ör. MIT, GPL-3.0) — seçilmeden ilk public push yapılmaz.
- Kod, yorum ve commit dili (açık kaynak için İngilizce önerilir) ve README dili.
- Derleme sistemi (Makefile mi CMake mi) ve CI (GitHub Actions + MSYS2 MINGW32).
- Servisin log hedefi (dosya mı, Event Log mu — Event Log mesaj kaynağı gerektirir).
