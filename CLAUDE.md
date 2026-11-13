# CLAUDE.md

Windows XP üzerinde SSD'lere TRIM gönderen bir araç paketi. MIT lisansıyla GitHub'da
açık kaynak olarak yayınlanacak. Dört bileşen:

| Bileşen | Dosya | Görev |
|---|---|---|
| Komut satırı aracı | `xptrim.exe` | info / trim (dry-run varsayılan) / install / uninstall |
| Servis | `xptrimsvc.exe` | Periyodik çalıştırma, `status.ini` yazma |
| Denetim Masası appleti | `xptrim.cpl` | Durum, ayarlar, şimdi çalıştır, tepsi simgesini aç/kapa |
| Tepsi simgesi (isteğe bağlı) | `xptrimtray.exe` | Saatin yanında durum; çift tık appleti açar |

Onaylı tasarım ve iş sırası [docs/plan.md](docs/plan.md) dosyasındadır; bir adım
tamamlandığında veya tasarım değiştiğinde aynı PR'da güncellenir.

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
  - XP'nin `msvcrt.dll`'i `%lld`, `%zu` gibi C99 biçimlerini desteklemez. Bu yüzden
    Makefile `__USE_MINGW_ANSI_STDIO=1` tanımlar (mingw'nin kendi printf'i statik bağlanır);
    `%I64d` gibi Microsoft'a özgü biçimler kullanılmaz.
- Derleme: MinGW-w64 i686 (msvcrt), `-D_WIN32_WINNT=0x0501 -DWINVER=0x0501`, statik link,
  PE subsystem version 5.01.
  - Bağlayıcı bayrakları: `-static -static-libgcc`,
    `-Wl,--major-subsystem-version,5,--minor-subsystem-version,1,--major-os-version,5,--minor-os-version,1`.
  - winpthreads ve C++ bağımlılığı çeken hiçbir şey kullanılmaz.
  - Uyarılar hata sayılır: `-std=c99 -Wall -Wextra -Werror`.
  - Yeni MinGW-w64 CRT sürümleri XP'de olmayan fonksiyonları içeri çekebilir. Bu yüzden
    `make check`, dağıtılan her exe/cpl'in import tablosunu `tools/allowed-imports.txt`
    ile karşılaştırır; listede olmayan bir import kontrolü başarısız sayar. Listeye yalnızca
    XP RTM'de (SP0) var olduğu doğrulanan fonksiyon eklenir, gerekçesi yanına yazılır.

## Komutlar

Geliştirme makinesi, kullanıcının Mac'inde Parallels içinde çalışan Windows 11 **ARM64**;
i686 exe'ler x86 emülasyonuyla çalışır, yani testler ana makinede koşar. Diskleri
Parallels'in sanal diskleridir (GPT): buradaki sonuçlar gerçek SSD davranışı hakkında bilgi
vermez. Claude'un kabuğu yönetici değildir; yönetici gerektiren salt-okunur denemeleri
kullanıcı kendisi çalıştırır. Toolchain: MSYS2 (`C:\msys64`) MINGW32 ortamı, GCC 16 i686.

```
tools\msys-run.cmd make         # build/xptrim.exe
tools\msys-run.cmd make test    # birim testleri
tools\msys-run.cmd make check   # XP import ve PE başlık kontrolü
```

PowerShell 5.1 native exe'lere iç tırnakları bozuk iletir; karmaşık bash komutları bir
betik dosyasına yazılıp çalıştırılır. CI (`.github/workflows/ci.yml`) aynı üç adımı koşar.

## Dil

- Kod, tanımlayıcılar, yorumlar, commit mesajları, PR'lar ve CHANGELOG İngilizce.
- README iki dilli: `README.md` (İngilizce) ve `README.tr.md` (Türkçe); biri değişince
  diğeri aynı PR'da güncellenir.
- Arayüz metinleri (applet, tepsi) string table'da EN + TR.
- Bu dosya (CLAUDE.md) Türkçe.

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
  - Bölüm tipi 0x42 (LDM / dinamik disk), 0x80 bitli tipler (NT FT set / RAID), GPT
    diskler ve dosya sistemi adı tam olarak `NTFS` olmayan birimler reddedilir.
  - Bölüm bilgisi ile disk extent'i (ofset ve uzunluk) birebir aynı, disk ve NTFS sektör
    boyutları eşit olmalıdır; tutarsızlık reddetme sebebidir. Bu kararlar
    `src/core/volcheck.c`'de, birim testleriyle korunur.
- Cihaz gerçekten TRIM destekliyorsa TRIM gönderilir: ATA IDENTIFY DEVICE word 169 bit 0
  kontrol edilir; tek komuttaki aralık sayısı word 105'e göre sınırlanır. Bilgi okunamıyorsa
  (USB köprüsü, bilinmeyen sürücü vb.) işlem yapılmaz.
- Boş alan yarışına karşı koruma: bağlı ve kilitlenmemiş bir birimde, bitmap anlık
  görüntüsünden alınan "boş" kümeler doğrudan TRIM'lenmez (okuma ile TRIM arasında dosya
  sistemi o kümeleri tahsis edebilir). Ya birim `FSCTL_LOCK_VOLUME` ile kilitlenir ya da boş
  alan önce araca ait bir yer tutucu dosyaya tahsis edilir ve yalnızca o dosyanın extent'leri
  (`FSCTL_GET_RETRIEVAL_POINTERS`) TRIM'lenir.
- ATA pass-through yolu (`IOCTL_ATA_PASS_THROUGH` ya da SAT ATA-16 CDB ile
  `IOCTL_SCSI_PASS_THROUGH`) SP sürümüne göre varsayılmaz; çalışma anında denenir,
  hiçbiri çalışmıyorsa işlem yapılmadan çıkılır. `SMART_RCV_DRIVE_DATA` yalnızca IDENTIFY
  için yedek yoldur, TRIM taşıyamaz. Pass-through ile gelen IDENTIFY verisi
  `xt_identify_parse` doğrulamasından geçmeden kabul edilmez.
- Pass-through IOCTL'leri, gönderilen komut salt-okunur IDENTIFY olsa bile fiziksel diskin
  okuma/yazma tutamacını ister. Bu yüzden `xptrim info` desteklenen bir birimde diski
  `GENERIC_READ | GENERIC_WRITE` açar; Claude bunu ana makinede yönetici olarak çalıştırmaz,
  denemeyi kullanıcı yapar.
- Testlerde veya örneklerde gerçek bir fiziksel diske asla yazılmaz.
  - Cihaza komut gönderen kod tek bir soyutlama katmanının (`device` arayüzü) arkasındadır;
    testler sahte (mock) cihaz kullanır ve gönderilen aralıkları kaydedip doğrular.
  - Dokümantasyondaki ve README'deki örnek komutlar `--apply` içermez.
  - Claude bu makinede `--apply` ile ya da `\\.\PhysicalDriveN` / `\\.\X:` üzerinde yazma
    yetkisiyle hiçbir şey çalıştırmaz. Gerçek donanımda doğrulama yalnızca kullanıcı
    tarafından, gözden çıkarılabilir bir diskte ve elle yapılır.

## Mimari

- `src/core/`    — saf mantık: LCN→LBA, aralık birleştirme/bölme, ATA DSM yükü, IDENTIFY
                   ayrıştırma, `status.ini` modeli. Win32 bağımlılığı yok, birim testli.
- `src/win32/`   — birim/disk sorguları, pass-through, yer tutucu dosya, yapılandırma, log,
                   servis kontrolü; `device` arayüzünün gerçek uygulaması.
- `src/cli/`, `src/service/`, `src/cpl/`, `src/tray/` — dört bileşen.
- `tests/`       — birim testleri ve sahte cihaz. `tools/` — derleme yardımcıları.
- Boş alan, yer tutucu bir dosyaya ayrılıp yalnızca onun extent'leri TRIM'lenerek işlenir;
  dry-run da aynı yolu izler (dosyayı oluşturup siler, ham diske yazmaz) ve apply modunda
  gönderilecek aralıkları birebir yazdırır.
- Yapılandırma: `%ProgramFiles%\xp-trim\xptrim.ini` (yalnızca yöneticiler yazar).
  Durum: servis `%ALLUSERSPROFILE%\Application Data\xp-trim\status.ini` yazar; applet ve
  tepsi yönetici gerektiren sorguları kendileri yapmaz, bu dosyayı okur.
- Servis LocalSystem olarak çalışır, `SERVICE_CONTROL_STOP`'a TRIM grupları arasında
  birkaç saniye içinde yanıt verir; işlemi yarıda bırakmak güvenlidir. "Şimdi çalıştır"
  özel kontrol kodu 128 ile gelir.
- Apply modu yalnızca CLI'da `--apply` ile ya da appletin yönetici onaylı onay kutusuyla
  (INI'de `apply=1`) açılır. Tepsi simgesi apply'ı açamaz.
- Applet `control.exe` içinde çalıştığı için XP görsel stili manifest kaynak kimliği 2 ve
  `ISOLATION_AWARE_ENABLED` ile etkinleştirilir.
- Çıkış kodları ve log biçimi belgelenir.

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
- Ana makine (Windows 11 ARM64) yalnızca derleme, birim testi ve salt-okunur sorgular
  içindir; üzerinde servis kurulmaz. "XP'de çalışıyor" iddiası ancak kullanıcının ayrı x86
  makinesinde/VM'inde (tercihen SP0 ve SP3) denendikten sonra yapılır; denenmemişse PR'da
  açıkça "XP'de doğrulanmadı" denir.
- Yayın: sürüm etiketleri `vX.Y.Z`; release ekinde exe'ler ve SHA-256 özetleri bulunur.

## Kararlar

- Lisans: MIT.
- Derleme: GNU Make + MSYS2 MINGW32; CI: GitHub Actions + `msys2/setup-msys2`.
- Servis logu: exe yanındaki döndürülen metin dosyası (Event Log mesaj kaynağı gerektirdiği
  için şimdilik yok).
