            /* 1. I-frame: LLC-заголовок (E6 E6 00 / E6 E7 00) есть всегда в
             *    первом (или единственном) кадре цепочки. Во 2-м, 3-м и
             *    последующих сегментах той же цепочки LLC-заголовка уже нет.
             * 2. S-frame: LLC-заголовка нет никогда (у этих кадров вообще
             *    отсутствует информационное поле).
             * 3. U-frame: LLC-заголовок есть только в кадрах типа UI
             *    (Unnumbered Information, байт Control = 0x03 или 0x13 с
             *    учетом P/F-бита). Во всех остальных U-frame (SNRM, UA, DISC,
             *    DM, FRMR) LLC-заголовка нет.
             */
            int expect_llc = 0;
            if (ctx->hdlc == hdlc) {
                switch (hdlc->control.type) {
                    case HDLC_S:
                        break;
                    case HDLC_U:
                        if (hdlc->control.u.cmd != HDLC_U_UI)
                            break;
                        __attribute__((fallthrough));
                    case HDLC_I:
                        expect_llc = 1;
                }
            }
            if (expect_llc) {
                /* DSAP - Destination Link Service Access Point */
                if (hdlc->payload_len < 3)
                    egoto2_set(E0, EBADMSG, "Ожидается 3 байта заголовка "
                        "llc, но размер пакета слишком мал (полезная "
                        "нагрузка: %d)", hdlc->payload_len);
                if (c != 0xe6)
                    egoto2_set(E0, EBADMSG, "Недопустимое значение Destination "
                        "LSAP: 0x%02x (Должно всегда быть 0xe6).", c);
                hdlc->llc_dsap = c;
                ctx->state = HDLC_LLC_DSAP;
            } else {
                ctx->state = HDLC_Payload_;
                if (hdlc->payload_len == 1)
                    ctx->state = HDLC_Payload;
            }

        } else if (ctx->state == HDLC_LLC_DSAP) {
            /* SSAP - Source Link Service Access Point */
            /* 0xe6 - request, 0xe7 - response */
            if (c != 0xe6 && c != 0xe7)
                egoto2_set(E0, EBADMSG, "Недопустимое значение Source LSAP: "
                    "0x%02x (Должно быть 0xe6 или 0xe7).", c);
            hdlc->llc_ssap = c;
            ctx->state = HDLC_LLC_SSAP;

        } else if (ctx->state == HDLC_LLC_SSAP) {
            /* 3-й байт — LLC Quality (Control): 0x00 — зарезервированный
             * байт качества обслуживания (в DLMS всегда равен нулю).
             */
            hdlc->llc_control = c;
            if (c)
                log("  [hdlc.llc] WARNING: LLC Quality/Control байт всегда должен быть 0, но получен 0x%02x.", c);
            ctx->state = (hdlc->payload_len == 3) ? HDLC_Payload : HDLC_LLC_Control;

        } else if (ctx->state == HDLC_LLC_Control || ctx->state == HDLC_Payload_) {
            ctx->state = HDLC_Payload_;
            assert(hdlc->payload_len);
            assert(mbuf_len(&hdlc->frame) <= 1 + hdlc->length - 2);
            if (mbuf_len(&hdlc->frame) == 1 + hdlc->length - 2)
                ctx->state = HDLC_Payload;


