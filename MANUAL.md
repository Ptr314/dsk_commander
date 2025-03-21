# Руководство пользователя

## Главное окно
<p align="center">
<img src="screenshots/main_window.png" width="600">
</p>

В левой части выводится список файлов-образов на диске, в правой &ndash; открытый образ. Вкладка &laquo;Файлы&raquo; показывает файлы, &laquo;Информация&raquo; &ndash; сводную информацию о файловой системе.  

## Основные операции

Левая панель:

* <img src="src/resources/icons/up.png" width="30"> &ndash; перейди вверх на один уровень;
* <img src="src/resources/icons/folder_yellow_open.png" width="30"> &ndash; открыть директорию;
* <img src="src/resources/icons/viewmag.png" width="30"> &ndash; анализ выбранного файла;
* <img src="src/resources/icons/5floppy_mount.png" width="30"> &ndash; открыть образ. Операцию также можно выполнить двойным кликом мышью.

Правая панель:

* <img src="src/resources/icons/msbox_info.png" width="30"> &ndash; информация о выбранном файле;
* <img src="src/resources/icons/binary.png" width="30"> &ndash; просмотр файла. Операцию также можно выполнить двойным кликом мышью.;
* <img src="src/resources/icons/3floppy_unmount.png" width="30"> &ndash; сохранить файл на диск;
* <img src="src/resources/icons/cdimage.png" width="30"> &ndash; Экспорт диска целиком.

Просмотр файла:

<p align="center">
<img src="screenshots/view_file.png" width="400">
</p>

В окне просмотра файла выбирается формат (Текст/двоичный) и кодировка.

Экспорт диска:

<p align="center">
<img src="screenshots/export.png" width="400">
</p>

При экспорте диска, в зависимости от выбранного формата, даступны следующие операции:

* Замена первых дорожек на дорожки из файла-образца. Пока доступно только для формата DSK.
* Указание метки тома. Доступно для физических форматов. Обратите внимание, что метка тома, указанная в файловой системе (можно посмотреть в окне информации), в общем случае должна совпадать со значением, записанным в заголовках секторов.
* Задать [чередование секторов](https://en.wikipedia.org/wiki/Interleaving_(disk_storage)) для физических форматов.