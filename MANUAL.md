# Руководство пользователя

## Главное окно
<p align="center">
<img src="screenshots/main_window.png" width="600">
</p>

В левой части выводится список файлов-образов на диске, в правой &ndash; открытый образ. Вкладка &laquo;Файлы&raquo; показывает файлы, &laquo;Информация&raquo; &ndash; сводную информацию о файловой системе.

Чтобы программа могла правильно обработать образы дисков, необходимо правильно установить их параметры. Во многих случаях это можно сделать автоматически, отметив опцию &laquo;Авто&raquo;, но в случае, когда автоопределение не сработало, отметку можно снять и попробовать задать значения вручную. 

## Основные операции

Левая панель:

* <img src="src/resources/icons/up.png" width="30"> &ndash; перейди вверх на один уровень;
* <img src="src/resources/icons/folder_yellow_open.png" width="30"> &ndash; открыть директорию;
* <img src="src/resources/icons/viewmag.png" width="30"> &ndash; анализ выбранного файла;
* <img src="src/resources/icons/5floppy_mount.png" width="30"> &ndash; открыть образ. Операцию также можно выполнить двойным кликом мышью.
* &laquo;Авто&raquo; &ndash; при открытии образа программа попытается определить параметры автоматически.

Правая панель:
* Блок &laquo;Список файлов&raquo;
    * <img src="src/resources/icons/up.png" width="30"> &ndash; перейди вверх на один уровень;
    * <img src="src/resources/icons/folder_font.png" width="30"> &ndash; включение/отключение алфавитной сортировки списка файлов;
    * <img src="src/resources/icons/trashcan_full.png" width="30"> &ndash; включение/отключение отображения удаленных файлов.
* Блок &laquo;Чтение и информация&raquo;
    * <img src="src/resources/icons/msbox_info.png" width="30"> &ndash; информация о выбранном файле;
    * <img src="src/resources/icons/binary.png" width="30"> &ndash; просмотр файла. Операцию также можно выполнить двойным кликом мышью.;
    * <img src="src/resources/icons/3floppy_unmount.png" width="30"> &ndash; сохранить файл на диск.
* Блок &laquo;Редактирование&raquo;
    * <img src="src/resources/icons/folder_new.png" width="30"> &ndash; создание директории;
    * <img src="src/resources/icons/package_editors.png" width="30"> &ndash; переименование файла, редактирование атрибутов и метаданных;
    * <img src="src/resources/icons/patch.png" width="30"> &ndash; добавление новых файлов;
    * <img src="src/resources/icons/delete.png" width="30"> &ndash; удаление файлов.
* Блок &laquo;Экспорт&raquo;
    * <img src="src/resources/icons/cdimage.png" width="30"> &ndash; экспорт диска целиком.

Просмотр файла:

<p align="center">
<img src="screenshots/view_file.png" width="400">
</p>

В окне просмотра файла выбирается формат (Текст/двоичный/Бейсик) и кодировка.

Экспорт диска:

<p align="center">
<img src="screenshots/export.png" width="400">
</p>

При экспорте диска, в зависимости от выбранного формата, доступны следующие операции:

* Замена первых дорожек на дорожки из файла-образца. Пока доступно только для формата DSK.
* Указание метки тома. Доступно для физических форматов. Обратите внимание, что метка тома, указанная в файловой системе (можно посмотреть в окне информации), в общем случае должна совпадать со значением, записанным в заголовках секторов.


Переименование файла и редактирование метаданных.

<p align="center">
<img src="screenshots/edit_metadata.png" width="400">
</p>

Набор доступных полей для редактирования зависит от файловой системы конкретного диска. На скриншоте выше показаны значения, характерные для Apple DOS 3.3.

С помощью переключателя вверху окна можно изменять формат вводимых числовых данных &ndash; десятичный/шестнадцатеричный.