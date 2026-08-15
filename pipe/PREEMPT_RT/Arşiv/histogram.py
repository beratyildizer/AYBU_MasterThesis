import sys
import os
import re
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.offsetbox import AnchoredText


if len(sys.argv) < 2:
    print("Usage: python histogram.py <input_file>")
    sys.exit(1)


filename = sys.argv[1]

latencies_ns = []

number_of_messages = "N/A"
payload_size = "N/A"

given_stats = {
    "min": None,
    "max": None,
    "average": None,
    "std": None,
    "median": None
}


def extract_number(line):
    match = re.search(r"[-+]?\d*\.?\d+", line)
    if match:
        return float(match.group(0))
    return None


def split_value_and_unit(text, default_unit=""):
    """
    Metni değer ve birim olarak ayırır.

    Örnek:
    "5000 bytes" -> ("5000", "bytes")
    "1000000"    -> ("1000000", "")
    "2782.11 ns" -> ("2782.11", "ns")
    "N/A"        -> ("N/A", "")
    """
    if text is None:
        return "N/A", ""

    text = str(text).strip()

    if text.upper() == "N/A":
        return "N/A", ""

    match = re.match(r"^\s*([-+]?\d*\.?\d+)\s*(.*)$", text)

    if match:
        value = match.group(1).strip()
        unit = match.group(2).strip()

        if not unit:
            unit = default_unit

        return value, unit

    return text, default_unit


with open(filename, "r") as file:
    for line in file:
        line = line.strip()

        if not line:
            continue

        # Baştaki # işaretini varsa kaldır
        clean_line = line.lstrip("#").strip()
        lower_line = clean_line.lower()

        # Header ve istatistik satırlarını oku
        if "number of message" in lower_line:
            number_of_messages = clean_line.split(":")[-1].strip()
            continue

        elif "payload size" in lower_line:
            payload_size = clean_line.split(":")[-1].strip()
            continue

        elif lower_line.startswith("min"):
            given_stats["min"] = extract_number(clean_line)
            continue

        elif lower_line.startswith("max"):
            given_stats["max"] = extract_number(clean_line)
            continue

        elif lower_line.startswith("average") or lower_line.startswith("mean"):
            given_stats["average"] = extract_number(clean_line)
            continue

        elif lower_line.startswith("standard deviation") or lower_line.startswith("std"):
            given_stats["std"] = extract_number(clean_line)
            continue

        elif lower_line.startswith("median"):
            given_stats["median"] = extract_number(clean_line)
            continue

        elif lower_line.startswith("total elapsed time"):
            continue

        elif "priority" in lower_line:
            continue

        elif "cpu affinity" in lower_line:
            continue

        # Geriye kalan satırlar latency değeri olmalı
        try:
            latencies_ns.append(float(clean_line))
        except ValueError:
            pass


latencies_ns = np.array(latencies_ns)

if len(latencies_ns) == 0:
    print("No latency values found in the file.")
    sys.exit(1)


# Python ile hesaplanan değerler
min_latency = np.min(latencies_ns)
max_latency = np.max(latencies_ns)
mean_latency = np.mean(latencies_ns)
median_latency = np.median(latencies_ns)
std_latency = np.std(latencies_ns)

calculated_stats = {
    "min": min_latency,
    "max": max_latency,
    "average": mean_latency,
    "std": std_latency,
    "median": median_latency
}


print("\nInput file:", filename)
print("Number of messages:", number_of_messages)
print("Payload size:", payload_size)

print("\nCalculated statistics from raw latency values:")
print(f"Minimum latency (ns)       : {min_latency:.3f}")
print(f"Maximum latency (ns)       : {max_latency:.3f}")
print(f"Average latency (ns)       : {mean_latency:.3f}")
print(f"Standard deviation (ns)    : {std_latency:.3f}")
print(f"Median latency (ns)        : {median_latency:.3f}")


# Karşılaştırma
tolerance = 0.01

print("\nComparison with given statistics in file:")

any_given_stat_found = False
mismatch_found = False

for key in ["min", "max", "average", "std", "median"]:
    given_value = given_stats[key]
    calculated_value = calculated_stats[key]

    if given_value is None:
        print(f"{key:<8}: No given value found in file.")
        continue

    any_given_stat_found = True
    difference = abs(given_value - calculated_value)

    if difference <= tolerance:
        print(
            f"{key:<8}: OK | given = {given_value:.3f}, "
            f"calculated = {calculated_value:.3f}, diff = {difference:.6f}"
        )
    else:
        mismatch_found = True
        print(
            f"{key:<8}: MISMATCH | given = {given_value:.3f}, "
            f"calculated = {calculated_value:.3f}, diff = {difference:.6f}"
        )


if not any_given_stat_found:
    print("\nComparison skipped: No previously calculated statistics were found in the file.")
elif mismatch_found:
    print("\nWARNING: Some given statistics do not match the values calculated from raw data.")
else:
    print("\nNo mismatch detected for available given statistics.")


# Çıktı dosya adı
base_name = os.path.splitext(os.path.basename(filename))[0]
output_image = "FIGURE_" + base_name + ".png"


# Bilgi kutusu için hizalama ayarları
label_width = 18
result_width = 14


def make_info_line(label, value, unit=""):
    """
    Değer ve birimi birlikte sağa yaslar.

    Örnek:
    Number of messages :        1000000
    Payload size       :     5000 bytes
    Min                :        2462 ns
    Max                :       17921 ns
    Average            :     3606.44 ns
    """
    if unit:
        result = f"{value} {unit}"
    else:
        result = f"{value}"

    return f"{label:<{label_width}} : {result:>{result_width}}"


# Number of messages -> sadece sayı
msg_value, msg_unit = split_value_and_unit(number_of_messages)

# Payload size -> sayı + bytes
payload_value, payload_unit = split_value_and_unit(payload_size, default_unit="bytes")

if payload_value != "N/A" and payload_unit == "":
    payload_unit = "bytes"


info_lines = [
    make_info_line("Number of messages", msg_value, msg_unit),
    make_info_line("Payload size", payload_value, payload_unit),
    make_info_line("Min", f"{min_latency:.0f}", "ns"),
    make_info_line("Max", f"{max_latency:.0f}", "ns"),
    make_info_line("Average", f"{mean_latency:.2f}", "ns"),
    make_info_line("Std. deviation", f"{std_latency:.2f}", "ns"),
    make_info_line("Median", f"{median_latency:.2f}", "ns")
]

info_text = "\n".join(info_lines)


plt.figure(figsize=(11, 7))

plt.hist(latencies_ns, bins=100, edgecolor="black")

plt.axvline(
    max_latency,
    linestyle="--",
    linewidth=2,
    label=f"Max = {max_latency:.0f} ns"
)

plt.xlim(0, max_latency * 1.05)
plt.yscale("log")

plt.title(f"Latency Histogram - {base_name}")
plt.xlabel("Latency (ns)")
plt.ylabel("Frequency (log scale)")

plt.legend()
plt.grid(True)


# Bilgi kutusu
anchored_text = AnchoredText(
    info_text,
    loc="upper right",
    prop=dict(
        size=10,
        family="DejaVu Sans Mono"
    ),
    frameon=True,
    borderpad=0.6
)

anchored_text.patch.set_boxstyle("round")
anchored_text.patch.set_facecolor("white")
anchored_text.patch.set_alpha(0.85)

plt.gca().add_artist(anchored_text)


plt.tight_layout()
plt.savefig(output_image, dpi=300)
plt.show()

print("\nHistogram saved as:", output_image)