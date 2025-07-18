# import tensorflow.lite as tflite

# # Load the TFLite model
# interpreter = tflite.Interpreter(model_path="detect.tflite")

# # Allocate memory for the model
# interpreter.allocate_tensors()

# # Get input and output details
# input_details = interpreter.get_input_details()
# output_details = interpreter.get_output_details()

# print("Input Details:", input_details)
# print("Output Details:", output_details)

import re
import cv2
import numpy as np
from tensorflow.lite.python.interpreter import Interpreter

CAMERA_WIDTH = 640
CAMERA_HEIGHT = 480

def load_labels(path='labels.txt'):
    """Loads the labels file. Supports files with or without index numbers."""
    with open(path, 'r', encoding='utf-8') as f:
        lines = f.readlines()
        labels = {}
        for row_number, content in enumerate(lines):
            pair = re.split(r'[:\s]+', content.strip(), maxsplit=1)
            if len(pair) == 2 and pair[0].strip().isdigit():
                labels[int(pair[0])] = pair[1].strip()
            else:
                labels[row_number] = pair[0].strip()
    return labels

def set_input_tensor(interpreter, image):
    """Sets the input tensor."""
    tensor_index = interpreter.get_input_details()[0]['index']
    input_tensor = interpreter.tensor(tensor_index)()[0]
    input_tensor[:, :] = np.expand_dims((image-255)/255, axis=0)

def get_output_tensor(interpreter, index):
    """Returns the output tensor at the given index."""
    output_details = interpreter.get_output_details()[index]
    tensor = np.squeeze(interpreter.get_tensor(output_details['index']))
    return tensor

def detect_objects(interpreter, image, threshold):
    """Returns a list of detection results, each a dictionary of object info."""
    set_input_tensor(interpreter, image)
    interpreter.invoke()
    # Get all output details
    boxes = get_output_tensor(interpreter, 1)
    classes = get_output_tensor(interpreter, 3)
    scores = get_output_tensor(interpreter, 0)
    count = int(get_output_tensor(interpreter, 2))  # Assuming count is a single value in the first element

    results = []
    for i in range(count):
        if scores[i] >= threshold:
            result = {
                'bounding_box': boxes[i],
                'class_id': classes[i],
                'score': scores[i]
            }
            results.append(result)
    return results

def process_test_image(image_path):
    """Process a test image and get detected objects."""
    image = cv2.imread(image_path)
    image_rgb = cv2.cvtColor(image, cv2.COLOR_BGR2RGB)
    image_resized = cv2.resize(image_rgb, (320, 320))
    return image, image_resized

def main():
    labels = load_labels()
    interpreter = Interpreter('detect.tflite')
    interpreter.allocate_tensors()

    # Debug: Print input tensor shape
    _, input_height, input_width, _ = interpreter.get_input_details()[0]['shape']
    print("Input tensor shape:", input_height, input_width)

    # Debug: Print output tensor details
    output_details = interpreter.get_output_details()
    for output in output_details:
        print(output['name'], output['shape'])

    # Path to your test image
    test_image_path = 'test\lowerlevel.79b845ca-0c59-11f0-8a67-08bfb8094c99.jpg'  # Replace with your image file path
    image, img_resized = process_test_image(test_image_path)
    
    # Get the detection results
    results = detect_objects(interpreter, img_resized, 0.5)

    for result in results:
        print(f"Class ID: {result['class_id']}, Score: {result['score']}")
        ymin, xmin, ymax, xmax = result['bounding_box']
        xmin = int(max(1, xmin * CAMERA_WIDTH))
        xmax = int(min(CAMERA_WIDTH, xmax * CAMERA_WIDTH))
        ymin = int(max(1, ymin * CAMERA_HEIGHT))
        ymax = int(min(CAMERA_HEIGHT, ymax * CAMERA_HEIGHT))

        cv2.rectangle(image, (xmin, ymin), (xmax, ymax), (0, 255, 0), 3)
        cv2.putText(image, labels[int(result['class_id'])], (xmin, min(ymax, CAMERA_HEIGHT-20)),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 2, cv2.LINE_AA)

    # Save or display the resulting image with bounding boxes and labels
    cv2.imwrite('result_image.jpg', image)
    cv2.imshow('Detected Objects', image)
    cv2.waitKey(0)
    cv2.destroyAllWindows()

if __name__ == "__main__":
    main()
