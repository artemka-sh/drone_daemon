#!/bin/bash
# Конвертация yolo12n.onnx -> yolo12n.om под OrangePi AIpro 20T (Ascend310B4)
#
# Перед запуском: положи yolo12n.onnx рядом (экспортируй через
# `yolo export model=yolo12n.pt format=onnx imgsz=640 opset=12 dynamic=False`
# если ещё не экспортировал — статическая форма важна, atc не всегда дружит
# с динамическими осями).

set -e

source /usr/local/Ascend/ascend-toolkit/latest/set_env.sh

atc --model=yolo12n.onnx \
    --framework=5 \
    --output=yolo12n \
    --input_format=NCHW \
    --input_shape="images:1,3,640,640" \
    --soc_version=Ascend310B4 \
    --insert_op_conf=aipp_yolo12.cfg \
    --output_type=FP32

# Если atc ругнётся на конкретный оператор (Einsum/Reshape в attention-блоках
# YOLO12) — пришли мне текст ошибки, там обычно нужен --op_select_implmode
# или замена оператора в самом ONNX перед конвертацией.
