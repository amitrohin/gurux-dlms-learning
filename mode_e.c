// vi: ts=4:sts=4:sw=4:et:nu:noai:nosi:syn=off

#define IOWAIT_RD   1
#define IOWAIT_WR   2

enum mode_e_state {
    MODE_E_Initial,
    MODE_E_7E1,
    MODE_E_Complete,
};
typedef struct mode_e_ctx {
    enum mode_e_state state;
    int ioflags;
    size_t msg_cap;
    size_t msg_len;
    size_t msg_pos;
    char *msg;
} MODE_E_CTX;

static inline void MODE_E_CTX_Init(MODE_E_CTX *ctx) {
    ctx->state = MODE_E_Initial;
}
static inline void MODE_E_CTX_Fini(MODE_E_CTX *ctx) {}

/* Функция возврвщает:
 * -1   - произошла ошибка.
 *          EAGAIN  - ввод-вывод не завершился, нужно продолжать.
 *          EINTR   - произошло прерывание вызова, продолжение возможно.
 *          все остальные ошибки
 *                  - нельзя продолжить ввод-вывод.
 *  0   - конец файла в неожиданном месте.
 *  1   - handshake выполнен.
 *
 *     Процесс состоит из двух этапов: сначала идет стандартное текстовое
 * "знакомство", а затем — переключение в бинарный режим.
 * 
 *     Начальное соединение (Текстовый режим): Оптический порт по умолчанию
 * работает на скорости 300 бод, 7 бит данных, четный паритет, 1 стоп-бит (300
 * 7E1) . Клиент отправляет запрос идентификации: /?![CR][LF].
 * 
 *     Ответ счетчика: Счетчик отвечает строкой идентификации. Ключевой момент —
 * наличие символа \2 (escape-последовательность). Если счетчик поддерживает HDLC,
 * он сообщает об этом в ответе (например, /ISK5\2ME383...) . Символ W=2 в строке
 * идентификации означает предложение перейти в Mode E .
 * 
 *     Запрос переключения: Клиент отправляет подтверждение (ACK) и выбирает
 * режим. Для перехода в бинарный режим используется команда ACK 2 Z 2 CR LF (где
 * Z — желаемая скорость, например, 9600) .
 * 
 *     Подтверждение и переход: Счетчик подтверждает переход. После этого
 * физический уровень (UART) перестраивается на новые параметры: 8 бит данных, без
 * паритета, 1 стоп-бит (8N1) . С этого момента линия считается "прозрачной" для
 * HDLC.
 * 
 *     Установление HDLC-соединения: Только после успешного переключения в Mode E
 * начинается стандартная процедура установления HDLC-соединения (обмен кадрами
 * SNRM/UA) и далее — установление ассоциации DLMS (AARQ/AARE) .
 */
int MODE_E_Update(MODE_E_CTX *ctx, FILE *fp) {
    int error = errno;

    if (ctx->state == MODE_E_sInitial) {
        struct termios tios;
        cfmakeraw(&tios);

        /* Кадр UART всегда состоит из:
         *
         * - Стартовый бит (всегда 1, всегда присутствует)
         * - Биты данных (5–9, обычно 7 или 8)
         * - Бит паритета (0 или 1 — можно отключить)
         * - Стоп-бит(ы) (1, 1.5 или 2 — минимум один обязателен)
         *
         * Режим порта - 7E1:
         *
         * 7 - 7 бит данных
         * Каждый передаваемый символ кодируется 7 битами (вместо обычных 8).
         *
         * E - Even (чётный паритет)
         * Это бит проверки чётности. Передатчик добавляет к 7 битам данных
         * ещё один бит так, чтобы общее количество единиц в 8 битах (7
         * данных + 1 паритет) было чётным. Приёмник проверяет это условие и
         * может обнаружить одиночную ошибку передачи. 'E' означает именно
         * чётный паритет (есть ещё N - нет паритета, O - нечётный).
         *
         * 1 - стоп-бит
         * После каждого символа передаётся один стоп-бит, который отделяет
         * один байт от другого и даёт приёмнику время на обработку.
         */
        tios.c_cflag |= PARENB;     /* добавляем бит parity */
        tios.c_cflag &= ~PARODD;    /* четный паритет (even parity) */
        tios.c_cflag &= ~CSTOPB;    /* сброс CSTOPB означает один стоповый
                                       бит, а не два */
        tios.c_cflag &= ~CSIZE;     /* очистим место для CS7 */
        tios.c_cflag |= CS7;        /* и установим семибитные данные */

        /* CREAD    - Enable Receiver (какой-то атавизм).
         * CLOCAL   - Ignore Modem Control Lines (игнорировать модемные
         *            сигналы (DCD — Carrier Detect, DSR, CTS и т.п.)
         */
        tios.c_cflag |= (CLOCAL | CREAD);

        if (cfsetspeed(&tios, B300))
            egoto(E0, errno, "cfsetspeed()");
        if (tcsetattr(fileno(fp), TSAFLUSH, &tios))
            egoto2(E0, errno, "tcsetattr()");
        ctx->state = MODE_E_7E1;
    }
    while (1) {
        int c = fgetc(fp);
        if (c < 0) {
            error = errno;
            if (ferror(fp)) {
                if (error == EAGAIN || error == EINTR)
                    clearerr(fp);
                else
                    elog2(error, "fgetc()");
                goto E1;
            }
            if (feof(fp)) {
                if (ctx->state == MODE_E_E71)
                    break;
                assert(ctx->mode != MODE_E_Complete);
                error = EBADMSG;
                egoto(E1, error, "fgetc(): Connection closed.")
            }
        }
        if (ctx->state == MODE_E_7E1) {
            ctx->msg = "/?!\r\n";
            ctx->msg_cap = 0;
            ctx->msg_len = 5;
            ctx->msg_pos = 0
        } else if (ctx->state == MODE_E_SEND) {
            n = fwrite(msg, 1, msg_len, fp);
            if (n < msg_len) {
                error = errno;
                if (ferror(fp)) {
                    if (error == )
                }
            }
        } else {
            elog("internal error");
            abort();
        }
    }
    return ctx->state != MODE_E_sInitial;

E1: errno = error;
E0: return -1;
}
