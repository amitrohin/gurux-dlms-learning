vi: ts=4:sts=4:sw=4:et:noai:nosi:nu:syn=off


[300 бод, 7E1]

    C -->   / ? $device_address ! CR LF

            / X X X $line_speed \ 2 $ident CR LF  <-- S

    C -->   ASK 2 $line_speed 2 CR LF

[$line_speed, 7E1]

            ASK 2 $line_speed 2 CR LF           <-- S   = 0x06 0xb2 0x35 0xb2 0x0d 0x0a

[$line_speed, 8N1]

    C -->   {Send data frame}
                            {Send data frame}   <-- S
    C -->   {Send break signal}



8.3.2 LLC PDU format
====================

The standard LLC PDU format is:

| Destination (remote) LSAP | Source (local) LSAP | Control      | Information |
+---------------------------+---------------------+--------------+-------------+
| 8 bits                    | 8 bits              | 8 or 16 bits | n*8 bits    |

DLMS/COSEM LLC PFU format is:

| Destination (remote) LSAP | Source (local) LSAP | LLC_Quality  | Information |
+---------------------------+---------------------+--------------+-------------+
| 0xe6                      | 0xe6 or 0xe7        | 0x00         | n*8 bits    |
                                ^       ^
                                |       +-- responce
                                +-- command

- The value of the Destination_LSAP is 0xE6;

- The value of the Source_LSAP is 0xE6 or 0xE7.  The least significant bit is
  used as a command/response identifier. When set to 0, it identifies a
  ‘command’ and when set to 1 it i dentifies a “response”;

- The Control byte is referred here to as the LLC_Quality parameter. It is
  reserved for future use. Its value is administered by the DLMS UA. Currently,
  it must be set always to 0x00;

- The information field consists of an integral number (including zero) of
  octets and it carries the LSDU.

The destination LSAP 0xFF is used for broadcasting purposes. Devices in this
environment shall never send messages with this broadcast address, but they
shall accept messages containing this broadcast destination address as if it
would be addressed to them.



8.4.1.1
HDLC frame format type 3
========================

| Flag  | Format | DA | SA | Control | HCS    | LLC + Data | FCS    | Flag  |
+-------+--------+----+----+---------+--------+------------+--------+-------+
| 8bits | 16bits |    |    | 8bits   | 16bits |            | 16bits | 8bits |
+-------+--------+----+----+---------+--------+------------+--------+-------+
| 0x7e  |                                                           | 0x7e  |

Flag
    0x7e

Frame format
    0b1010 S LLLLLLLLLLL
      ^    ^      ^
      |    |      +-- (11 bits) Frame length sub-field
      |    +-- (1 bits) segmentation-bit
      +-- (4 bits) format-type (identifies a frame format type 3)

DA & SA
    Upper HDLC address - logical device
    Lower HDLC address - physical device
                |   8 bits    |   8 bits    |   8 bits    |   8 bits   |
                |      7    |1|      7    |1|      7    |1|      7   |1|
                +-------------+-------------+-------------+------------+
    - 1 байт    |Upper      |1|
                +-------------+-------------+
    - 2 байта   |Upper      |0|Lower      |1|
                +-------------+-------------+-------------+------------+
    - 4 байта   |Upper, high|0|Uppper, low|0|Lower, high|0|Lower, low|1|
                +-------------+-------------+-------------+------------+

    [Reserved HDLC client addresses]
    No-station                      0x00
    Client Management Process       0x01
    Public Client                   0x10
    Open for client SAP assignment  0x02..0x0F, 0x11..0xFF

    [Reserved upper HDLC server addresses]
                                    One byte address    Two byte address
    -------------------------------+-------------------+----------------
    No-station                      0x00                0x0000
    Management Logical Device       0x01                0x0001
    Reserved for future use         0x02..0x0F          0x0002..0x000F
    Open for server SAP assignment  0x10..0x7E          0x0010..0x3FFE
    All-station (Broadcast)         0x7F                0x3FFF

    [Reserved lower HDLC server addresses]
                                    One byte address    Two byte address
    -------------------------------+-------------------+----------------
    No-station                      0x00                0x0000
    Reserved for future use         0x01..0x0F          0x0001..0x000F
    Open for server SAP assignment  0x10..0x7D          0x0010..0x3FFD
    CALLING Physical Device         0x7E                0x3FFE
    All-station (Broadcast)         0x7F                0x3FFF

    [!] Клиентский адрес всегда 1 байт. Серверный адрес (счетчик) может быть
        1, 2 или 4 байта.

Control         - ??? 8.4.3

HCS             - chksum(FrameFormat + DA + SA + Control)

Information     - LLC + DATA
    [!] Поля HCS и Information отсутствуют, если Information пустое (0 байт).

FCS             - chksum(FrameFormat + DA + SA + Control + HCS + Information)





[Control field bit assignments of command and response frames]

                           MSB             LSB
Command         Response
---------------+-----------+-----------------
I               I           R R R P/F S S S 0
RR              RR          R R R P/F 0 0 0 1
RNR             RNR         R R R P/F 0 1 0 1
SNRM                        1 0 0  P  0 0 1 1
DISC                        0 1 0  P  0 0 1 1
                UA          0 1 1  F  0 0 1 1
                DM          0 0 0  F  1 1 1 1
                FRMR        1 0 0  F  0 1 1 1
UI              UI          0 0 0 P/F 0 0 1 1

RRR     - the receive sequence number N(R)
SSS     - the send sequence number N(S)
P/F     - the poll/final bit


8.4.3.2 Команда и ответ передачи информации

Функция кадра команды и ответа передачи информации – I-кадра – состоит в
выполнении передачи информации. Управляющее поле I-кадра должно содержать два
порядковых номера:

a) N(S), который должен указывать порядковый номер, связанный с I-кадром; и
b) N(R), который должен указывать порядковый номер (на момент передачи)
следующего ожидаемого к приёму I-кадра и, следовательно, должен указывать, что
I-кадры с номерами до N(R) – 1 включительно были приняты правильно.

По причинам целостности данных в данном профиле значение по умолчанию для
параметров HDLC «максимальная длина информационного поля – приём» и
«максимальная длина информационного поля – передача» составляет 128 байт.
Другие значения могут быть согласованы во время установления соединения, см.
8.4.5.3.1.

ПРИМЕЧАНИЕ 1. Для обеспечения минимальной производительности главная станция
должна предлагать max_info_field_length_receive не менее 128 байт.

ПРИМЕЧАНИЕ 2. Максимальное значение длины информационного поля составляет 2030
байт.

Бит P/F, который может быть установлен в «1» или «0». Его использование
поясняется в 8.4.4.1.
