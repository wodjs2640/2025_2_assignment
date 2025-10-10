import cv2
import numpy as np
import matplotlib.pyplot as plt


def get_differential_filter():
    # TODO: implement this function
    filter_x = np.array([[0, 0, 0], [-1, 0, 1], [0, 0, 0]], dtype=np.float32)
    filter_y = np.array([[0, -1, 0], [0, 0, 0], [0, 1, 0]], dtype=np.float32)
    return filter_x, filter_y


def filter_image(im, filter):
    # TODO: implement this function
    k = filter.shape[0]
    pad = k // 2
    H, W = im.shape
    im_padded = np.pad(im, ((pad, pad), (pad, pad)), mode="constant")
    out = np.zeros_like(im, dtype=np.float32)

    for y in range(H):
        for x in range(W):
            patch = im_padded[y : y + k, x : x + k]
            out[y, x] = float(np.sum(patch * filter))
    return out


def get_gradient(im_dx, im_dy):
    # TODO: implement this function
    grad_mag = np.hypot(im_dx, im_dy).astype(np.float32)
    grad_angle = np.mod(np.arctan2(im_dy, im_dx), np.pi).astype(np.float32)
    return grad_mag, grad_angle


def build_histogram(grad_mag, grad_angle, cell_size):
    # TODO: implement this function
    H, W = grad_mag.shape
    M, N = H // cell_size, W // cell_size
    ori_histo = np.zeros((M, N, 6), dtype=np.float32)

    bin_width = np.pi / 6.0
    offset = np.pi / 12.0
    shifted = (grad_angle + offset) % np.pi
    bin_idx_full = np.floor(shifted / bin_width).astype(np.int32)

    for i in range(M):
        for j in range(N):
            y0, y1 = i * cell_size, (i + 1) * cell_size
            x0, x1 = j * cell_size, (j + 1) * cell_size
            mags = grad_mag[y0:y1, x0:x1]
            bins = bin_idx_full[y0:y1, x0:x1]
            for k in range(6):
                ori_histo[i, j, k] = float(mags[bins == k].sum())
    return ori_histo


def get_block_descriptor(ori_histo, block_size):
    # TODO: implement this function
    M, N, _ = ori_histo.shape
    b = block_size
    out_h, out_w = M - b + 1, N - b + 1
    D = 6 * (b**2)
    out = np.zeros((out_h, out_w, D), dtype=np.float32)
    eps = 1e-3

    for i in range(out_h):
        for j in range(out_w):
            block = ori_histo[i : i + b, j : j + b, :].reshape(-1)
            norm = np.sqrt(np.sum(block**2) + eps**2)
            out[i, j, :] = block / norm
    return out


# visualize histogram of each block
def visualize_hog(im, hog, cell_size, block_size):
    num_bins = 6
    max_len = (
        7  # control sum of segment lengths for visualized histogram bin of each block
    )
    im_h, im_w = im.shape
    num_cell_h, num_cell_w = int(im_h / cell_size), int(im_w / cell_size)
    num_blocks_h, num_blocks_w = (
        num_cell_h - block_size + 1,
        num_cell_w - block_size + 1,
    )
    histo_normalized = hog.reshape(
        (num_blocks_h, num_blocks_w, block_size**2, num_bins)
    )
    histo_normalized_vis = (
        np.sum(histo_normalized**2, axis=2) * max_len
    )  # num_blocks_h x num_blocks_w x num_bins
    angles = np.arange(0, np.pi, np.pi / num_bins)
    mesh_x, mesh_y = np.meshgrid(
        np.r_[cell_size : cell_size * num_cell_w : cell_size],
        np.r_[cell_size : cell_size * num_cell_h : cell_size],
    )
    mesh_u = histo_normalized_vis * np.sin(angles).reshape(
        (1, 1, num_bins)
    )  # expand to same dims as histo_normalized
    mesh_v = histo_normalized_vis * -np.cos(angles).reshape(
        (1, 1, num_bins)
    )  # expand to same dims as histo_normalized
    plt.imshow(im, cmap="gray", vmin=0, vmax=1)
    for i in range(num_bins):
        plt.quiver(
            mesh_x - 0.5 * mesh_u[:, :, i],
            mesh_y - 0.5 * mesh_v[:, :, i],
            mesh_u[:, :, i],
            mesh_v[:, :, i],
            color="red",
            headaxislength=0,
            headlength=0,
            scale_units="xy",
            scale=1,
            width=0.002,
            angles="xy",
        )
    # plt.show()
    plt.savefig("hog.png")


def extract_hog(im, visualize=False, cell_size=8, block_size=2):
    # TODO: implement this function
    im = im.astype(np.float32) / 255.0

    fx, fy = get_differential_filter()

    im_dx = filter_image(im, fx)
    im_dy = filter_image(im, fy)

    grad_mag, grad_angle = get_gradient(im_dx, im_dy)

    ori_histo = build_histogram(grad_mag, grad_angle, cell_size)

    ori_histo_norm = get_block_descriptor(ori_histo, block_size)

    hog = ori_histo_norm.reshape(-1)

    if visualize:
        visualize_hog(im, hog, cell_size, block_size)
    return hog


def _nms_keep(boxes_xyxy, scores, iou_thresh=0.5):
    if len(boxes_xyxy) == 0:
        return []

    boxes = np.array(boxes_xyxy, dtype=np.float32)
    scores = np.array(scores, dtype=np.float32)
    order = scores.argsort()[::-1]

    keep = []
    x1, y1, x2, y2 = boxes.T
    areas = (x2 - x1 + 1) * (y2 - y1 + 1)

    while order.size > 0:
        i = order[0]
        keep.append(i)

        xx1 = np.maximum(x1[i], x1[order[1:]])
        yy1 = np.maximum(y1[i], y1[order[1:]])
        xx2 = np.minimum(x2[i], x2[order[1:]])
        yy2 = np.minimum(y2[i], y2[order[1:]])

        w = np.maximum(0.0, xx2 - xx1 + 1)
        h = np.maximum(0.0, yy2 - yy1 + 1)
        inter = w * h
        iou = inter / (areas[i] + areas[order[1:]] - inter + 1e-6)

        inds = np.where(iou <= iou_thresh)[0]
        order = order[inds + 1]
    return keep


def face_recognition(
    I_target, I_template, cell_size=8, block_size=2, score_thresh=0.37, iou_thresh=0.5
):
    # TODO: implement this function
    tgt = I_target.astype(np.float32)
    tmpl = I_template.astype(np.float32)
    if tgt.ndim == 3:
        tgt = cv2.cvtColor(tgt, cv2.COLOR_BGR2GRAY)
    if tmpl.ndim == 3:
        tmpl = cv2.cvtColor(tmpl, cv2.COLOR_BGR2GRAY)

    th, tw = tmpl.shape
    a = extract_hog(tmpl, visualize=False, cell_size=cell_size, block_size=block_size)
    a0 = a - a.mean()
    a_norm = np.linalg.norm(a0) + 1e-6

    H, W = tgt.shape
    step = cell_size
    boxes, scores = [], []

    for y in range(0, H - th + 1, step):
        for x in range(0, W - tw + 1, step):
            patch = tgt[y : y + th, x : x + tw]
            b = extract_hog(
                patch, visualize=False, cell_size=cell_size, block_size=block_size
            )
            if b.shape != a.shape:
                continue
            b0 = b - b.mean()
            b_norm = np.linalg.norm(b0) + 1e-6
            s = float(np.dot(a0, b0) / (a_norm * b_norm))  # NCC
            if s >= score_thresh:
                boxes.append([x, y, x + tw - 1, y + th - 1])
                scores.append(s)

    keep = _nms_keep(boxes, scores, iou_thresh=iou_thresh)
    boxes = [boxes[i] for i in keep]
    scores = [scores[i] for i in keep]

    out = np.zeros((len(boxes), 3), dtype=np.float32)
    for i, (bx, by, ex, ey) in enumerate(boxes):
        out[i] = [bx, by, scores[i]]
    return out


def visualize_face_detection(I_target, bounding_boxes, box_size):

    hh, ww, cc = I_target.shape

    fimg = I_target.copy()
    for ii in range(bounding_boxes.shape[0]):

        x1 = bounding_boxes[ii, 0]
        x2 = bounding_boxes[ii, 0] + box_size
        y1 = bounding_boxes[ii, 1]
        y2 = bounding_boxes[ii, 1] + box_size

        if x1 < 0:
            x1 = 0
        if x1 > ww - 1:
            x1 = ww - 1
        if x2 < 0:
            x2 = 0
        if x2 > ww - 1:
            x2 = ww - 1
        if y1 < 0:
            y1 = 0
        if y1 > hh - 1:
            y1 = hh - 1
        if y2 < 0:
            y2 = 0
        if y2 > hh - 1:
            y2 = hh - 1
        fimg = cv2.rectangle(
            fimg, (int(x1), int(y1)), (int(x2), int(y2)), (255, 0, 0), 1
        )
        cv2.putText(
            fimg,
            "%.2f" % bounding_boxes[ii, 2],
            (int(x1) + 1, int(y1) + 2),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.5,
            (0, 255, 0),
            2,
            cv2.LINE_AA,
        )

    plt.figure(3)
    plt.imshow(fimg, vmin=0, vmax=1)
    plt.imsave("result_face_detection.png", fimg, vmin=0, vmax=1)
    plt.show()


if __name__ == "__main__":

    im = cv2.imread("cameraman.tif", 0)
    hog = extract_hog(im, visualize=True)

    I_target = cv2.imread("target.png", 0)  # MxN image

    I_template = cv2.imread("template.png", 0)  # mxn  face template

    bounding_boxes = face_recognition(I_target, I_template)

    I_target_c = cv2.imread("target.png")  # MxN image (just for visualization)

    visualize_face_detection(
        I_target_c, bounding_boxes, I_template.shape[0]
    )  # visualization code
