# xp-trim

[English](README.md)

Windows XP (SP0–SP3, 32-bit) için SSD TRIM aracı.

> **Durum: erken geliştirme aşaması. Bu depodaki hiçbir şey henüz TRIM göndermiyor.**
> Verisi önemli olan bir diskte kullanmayın.

Windows XP, TRIM'den önce çıktığı için XP altında kullanılan SSD'ler hangi blokların boş
olduğunu hiç öğrenemez ve yazma performansları zamanla düşer. xp-trim bu boşluğu doldurur:

| Bileşen | Dosya | Görev |
|---|---|---|
| Komut satırı aracı | `xptrim.exe` | Birimleri incelemek ve boş alanı TRIM'lemek (varsayılan dry-run) |
| Servis | `xptrimsvc.exe` | TRIM'i zamanlanmış olarak çalıştırır |
| Denetim Masası appleti | `xptrim.cpl` | Durumu görme, ayarları değiştirme, şimdi çalıştırma |
| Bildirim alanı simgesi (isteğe bağlı) | `xptrimtray.exe` | Saatin yanında durum; çift tık appleti açar |

## Güvenlik tasarımı

- **Varsayılan mod dry-run'dır.** Gerçek TRIM komutları yalnızca `--apply` ile
  (ya da servis yapılandırmasında bir yöneticinin ayarladığı `apply=1` ile) gönderilir.
- Boş alan önce geçici bir yer tutucu dosyayla ayrılır ve yalnızca o dosyanın kümeleri
  TRIM'lenir; böylece dosya sistemi işlem sırasında bu kümeleri yeniden kullanamaz.
- Bölüm ofseti ve sektör boyutu her zaman sistemden okunur, asla varsayılmaz.
- Dinamik diskler, RAID/spanned birimler, GPT diskler ve NTFS olmayan birimler reddedilir.
- TRIM yalnızca sürücü TRIM desteğini bildiriyorsa (ATA IDENTIFY word 169) gönderilir.

## Kullanım

Yönetici olarak açılmış bir komut isteminden çalıştırın.

```
xptrim info C:      C: biriminin TRIM'lenip TRIM'lenemeyeceğini gösterir (salt-okunur)
xptrim --version
```

| Çıkış kodu | Anlamı |
|---|---|
| 0 | Başarılı (`info`: birim destekleniyor) |
| 2 | Kullanım hatası |
| 3 | Birim desteklenmiyor; nedeni yazdırılır |
| 4 | Bir sistem sorgusu başarısız oldu; adım ve Windows hata kodu yazdırılır |

## Derleme

MINGW32 ortamıyla birlikte [MSYS2](https://www.msys2.org/) gerekir:

```sh
pacman -S --needed mingw-w64-i686-gcc mingw-w64-i686-binutils make
make          # build/xptrim.exe
make test     # birim testleri
make check    # exe'lerin yalnızca XP SP0'da bulunan fonksiyonları kullandığını doğrular
```

cmd veya PowerShell'den `tools\msys-run.cmd make test` aynı komutu MSYS2 içinde çalıştırır.

Exe'ler XP'nin `msvcrt.dll`'ine karşı statik bağlanır, PE işletim sistemi/alt sistem
sürümleri 5.01'dir ve ek bir çalışma zamanı kurulumu gerektirmez.

## Lisans

[MIT](LICENSE)
