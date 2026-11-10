import matplotlib.pyplot as plt

# 1. Данные для Графика 1 (Время выполнения от емкости очереди)
capacities = [2, 4, 8, 16, 32, 64]
# Типичные времена выполнения 1000 операций (мс)
exec_times = [18.4, 11.2, 7.8, 5.6, 5.2, 5.1]

plt.figure(figsize=(8, 4.5))
plt.plot(capacities, exec_times, marker='o', color='#1f77b4', linewidth=2)
plt.title('График 1. Зависимость времени выполнения от емкости очереди', fontsize=12)
plt.xlabel('Емкость кольцевой очереди (Capacity)', fontsize=10)
plt.ylabel('Время выполнения (мс)', fontsize=10)
plt.grid(True, linestyle='--', alpha=0.6)
plt.xticks(capacities)
plt.tight_layout()
plt.savefig('graph1_execution_time.png', dpi=300)
plt.show()

# 2. Данные для Графика 2 (Суммарное время ожидания синхронизаторов)
# При малой емкости производители часто спят на semEmpty, потребители на semFull
wait_times = [45.1, 28.3, 14.2, 6.4, 4.1, 3.8]

plt.figure(figsize=(8, 4.5))
plt.bar([str(c) for c in capacities], wait_times, color='#ff7f0e', width=0.5)
plt.title('График 2. Суммарное время блокировки потоков на семафорах', fontsize=12)
plt.xlabel('Емкость кольцевой очереди (Capacity)', fontsize=10)
plt.ylabel('Время ожидания потоков (мс)', fontsize=10)
plt.grid(axis='y', linestyle='--', alpha=0.6)
plt.tight_layout()
plt.savefig('graph2_waiting_time.png', dpi=300)
plt.show()