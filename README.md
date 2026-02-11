# DISK Commander

[![GitHub Release](https://img.shields.io/github/release/ptr314/dsk_commander.svg?style=flat)]() 

DISK Commander &ndash; программа для просмотра, редактирования, анализа и конвертации файлов образов дискет ретро-компьютеров.


* [Скачать последнюю версию](https://github.com/Ptr314/dsk_commander/releases)
* [Руководство пользователя](MANUAL.md)
* [Группа в Телеграме](https://t.me/ecat_emu)
* [Emuverse.ru](https://emuverse.ru) &ndash; энциклопедия эмуляции на русском языке.

<p align="center">
<img src="screenshots/main_window.png" width="600">
</p>

На данный момент поддерживается просмотр дисков следующих форматов:
* Агат / Apple II 140 Кб, (DSK, NIC, MFM, NIB).
* Агат 840 Кб (DSK, AIM, HFE, NIB).
* Ириша 360 Кб (DSK, с чередованием дорожек и без).

Редактирование образов пока поддерживается только для дисков DOS 3.3.

Файловые системы:

* DOS 3.3 / Агат:
    * Экспорт отдельных файлов с диска в двоичный формат и формат [.FIL](http://agatcomp.ru/agat/PCutils/FileType/FIL.shtml).
    * Добавление/удаление файлов из форматов .FIL и двоичного, добавление/удаление директорий.
    * Редактирование атрибутов файлов.
* ОС &laquo;Спрайт&raquo;:
    * Экспорт в двоичный формат.
* CP/M (с чередованием секторов DOS 3.3, ProDOS, без чередования):
    * Экспорт в двоичный формат.

Просмотр файлов на дисках:
* Шестнадцатеричный дамп.
* Текст с переключением кодировок Агат, Apple, ASCII.
* Бейсик (Агат / Apple / CP/M MBASIC).
* Образы экранов графических и текстовых режимов Агат, файлов знакогенераторов.
  
Импорт/экспорт файлов-образов:
* DSK (посекторные образы)
    * 140 и 840К 
* DO, PO, CPM (посекторные образы для Apple CP/M)
    * 140К 
* HFE (физический формат для [эмулятора Готек](https://www.gotekemulator.com/))
    * 840К 
* NIC (физический формат для [&laquo;японского&raquo; эмулятора](https://tulip-house.ddo.jp/digital/SDISK2/english.html))
    * 140К 
* [AIM](http://agatcomp.ru/agat/PCutils/FileType/AIM.shtml) (физический формат дисков 840К для Агата)
    * 840К 
* MFM, NIB (физический формат)
    * 140К
    * 840K

<hr>

Благодарности:

* David Vignoni за коллекцию иконок [Nuvola](https://commons.wikimedia.org/wiki/Category:Nuvola_icons);
* Сообществу сайта [agatcomp.ru](http://agatcomp.ru) за помощь с информацией.