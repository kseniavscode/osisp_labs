import matplotlib.pyplot as plt
import numpy as np

# Конфигурации: А (1 клиент, 1 КБ), Б (3 клиента, 1 КБ), В (3 клиента, 64 КБ)
configs = ['А (1 клик, 1 КБ)', 'Б (3 клик, 1 КБ)', 'В (3 клик, 64 КБ)']
# Типичные времена выполнения 300 запросов (мс)
times = [312.4, 128.6, 218.5]
# Пропускная способность (КБ/с)
throughput = [(300 * 1.0) / (times[0] / 1000.0),
              (300 * 1.0) / (times[1] / 1000.0),
              (300 * 64.0) / (times[2] / 1000.0)]

# График 1: Время выполнения
plt.figure(figsize=(7, 4.5))
bars = plt.bar(configs, times, color=['#1f77b4', '#2ca02c', '#d62728'], width=0.5)
plt.title('График 1. Время выполнения серии из 300 запросов (мс)', fontsize=11)
plt.ylabel('Время (мс)', fontsize=10)
plt.grid(axis='y', linestyle='--', alpha=0.7)
for bar in bars:
    yval = bar.get_height()
    plt.text(bar.get_x() + bar.get_width()/2.0, yval + 5, f'{yval:.1f} мс', ha='center', va='bottom', fontsize=9)
plt.tight_layout()
plt.savefig('graph1_execution_time.png', dpi=300)
plt.close()

# График 2: Пропускная способность
plt.figure(figsize=(7, 4.5))
bars2 = plt.bar(configs, throughput, color=['#ff7f0e', '#9467bd', '#8c564b'], width=0.5)
plt.title('График 2. Пропускная способность сетевого взаимодействия (КБ/с)', fontsize=11)
plt.ylabel('Скорость передачи (КБ/с)', fontsize=10)
plt.grid(axis='y', linestyle='--', alpha=0.7)
for bar in bars2:
    yval = bar.get_height()
    plt.text(bar.get_x() + bar.get_width()/2.0, yval + 1000, f'{yval:.0f} КБ/с', ha='center', va='bottom', fontsize=9)
plt.tight_layout()
plt.savefig('graph2_throughput.png', dpi=300)
plt.close()
print("Графики graph1_execution_time.png и graph2_throughput.png успешно созданы.")