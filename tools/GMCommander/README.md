# GM Commander (BFA-Core)

SOAP ile GM komutu gönderen ve world veritabanına bağlanıp **waypoint** düzenleyen masaüstü programı.
Python 3 + Tkinter; tek ek bağımlılık `pymysql`.

## Kurulum

1. Python 3.9+ kurun (Windows'ta kurulumda "Add to PATH" ve "tcl/tk" seçili olsun).
2. Windows: `start.bat` dosyasına çift tıklayın. Linux: `./start.sh`.
   (Elle: `pip install pymysql` ve `python gmcommander.py`)

## Sunucu tarafı

`worldserver.conf` içinde:

```
SOAP.Enabled = 1
SOAP.IP      = "127.0.0.1"   # Başka makineden bağlanacaksanız "0.0.0.0"
SOAP.Port    = 7878
```

SOAP hesabı GM seviyesi **3** olmalı (worldserver konsolunda):

```
account set gmlevel 1#1 3 -1
```

BFA'da oyun hesabı adı genelde `1#1` biçimindedir (bnet hesabı #1'in ilk oyun hesabı).

## Sekmeler

| Sekme | Ne işe yarar |
|---|---|
| **GM Konsolu** | Herhangi bir GM komutunu SOAP ile çalıştırır. Yukarı/aşağı ok = komut geçmişi. Sağdaki hazır komutlar kutuya şablon yazar (`<...>` kısmı seçili gelir). Hazır komut listesi `config.json` → `quick_commands` içinden değiştirilebilir. |
| **Waypoint Editörü** | `waypoint_data` tablosundaki path'leri yükler, düzenler, kaydeder. |
| **Creature Ara** | İsim, entry veya GUID ile creature arar; çift tık = waypoint editöründe aç. |
| **SQL** | world veya characters DB'de serbest sorgu. Değiştiren sorgular onay ister. |
| **Ayarlar** | SOAP ve MySQL bilgileri; `config.json`'a kaydedilir (şifreler düz metin, dosyayı paylaşmayın). |

## Waypoint akışı

1. Creature GUID girin → **Bul** → **Creature'ın path'ini yükle**
   (path yoksa yeni Path ID önerir: TrinityCore geleneği `guid * 10`).
2. Nokta ekleme yolları:
   - Formu doldurup **Ekle** / **Seçilenin altına ekle**
   - Oyunda `.gps` yazıp çıktıyı **Koordinat yapıştır** kutusuna yapıştırın (birden çok nokta olabilir),
     ya da `x y z [o]` satırları
   - **Karakter** adını yazıp **Konumunu nokta olarak ekle**: SOAP ile `saveall` yapar, sonra
     karakterin konumunu characters DB'den okur. Oyunda yürüyüp her noktada tıklayarak path çizebilirsiniz.
3. Noktaları tablodan seçip taşıyın/silin; toplu olarak bekleme, hareket tipi (yürü/koş/uç), Z düzeltmesi
   veya "Orientation'ları hesapla" (her nokta bir sonrakine bakar).
4. **DB'ye KAYDET** → noktalar 1..n olarak yeniden numaralanır, istenirse SOAP ile `reload waypoint_data`.
5. **Bu path'i creature'a ata** → `creature_addon.path_id` ve `creature.MovementType = 2`
   (istenirse spawn noktası 1. noktaya taşınır).
6. Oyunda creature'ı respawn edin (veya `.wp reload <pathId>`), yeni yolu yürür.

**SQL dışa aktar** ile path'i (ve creature ataması varsa onu da) `.sql` dosyası olarak alıp
`sql/updates` altına koyabilirsiniz.

## Notlar

- `waypoint_data.move_type`: 0 yürü, 1 koş, 2 yere in, 3 havalan.
- `action` alanı `waypoint_scripts.id`'ye bağlanır; `action_chance` yüzde olarak çalışma şansıdır.
- Önizlemede yukarı = kuzey (+X), yeşil = ilk nokta, kırmızı = son nokta, sarı kare = creature spawn'ı.
- SOAP portunu internete açmayın; uzaktan kullanacaksanız güvenlik duvarıyla IP kısıtlayın veya SSH tüneli kullanın.
