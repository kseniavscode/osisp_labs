import matplotlib.pyplot as plt

sizes = [100, 1000, 10000]
# Фактические средние времена выполнения (мс)
exec_times = [18.42, 172.65, 1715.30]
# Пропускная способность (миллионов операций с элементами в секунду)
throughput = [(s * 10000) / (t / 1000.0) / 1e6 for s, t in zip(sizes, exec_times)]

plt.figure(figsize=(7, 4.5))
plt.plot(sizes, exec_times, marker='o', color='#1f77b4', linewidth=2)
plt.xscale('log')
plt.yscale('log')
plt.title('График 1. Зависимость времени обработки от размера массива', fontsize=11)
plt.xlabel('Количество элементов массива (log-шкала)', fontsize=10)
plt.ylabel('Время выполнения 10 000 итераций, мс (log-шкала)', fontsize=10)
plt.grid(True, which="both", ls="--", alpha=0.6)
plt.tight_layout()
plt.savefig('graph1_execution_time.png', dpi=300)
plt.close()

plt.figure(figsize=(7, 4.5))
bars = plt.bar([str(s) for s in sizes], throughput, color='#2ca02c', width=0.45)
plt.title('График 2. Пропускная способность обработки элементов (млн эл/сек)', fontsize=11)
plt.xlabel('Количество элементов массива', fontsize=10)
plt.ylabel('Млн элементов / сек', fontsize=10)
plt.grid(axis='y', linestyle='--', alpha=0.6)
for bar in bars:
    y = bar.get_height()
    plt.text(bar.get_x() + bar.get_width()/2.0, y + 1, f'{y:.1f}', ha='center', va='bottom', fontsize=9)
plt.tight_layout()
plt.savefig('graph2_throughput.png', dpi=300)
plt.close()
print("Графики graph1_execution_time.png и graph2_throughput.png созданы.")