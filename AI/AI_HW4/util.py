import torch
import matplotlib.pyplot as plt
import os

default_config = {
    "Batch_size":128,
    "Num_epoch":30,
    "Learning_rate":0.1,
    "Scheduler":"MultiStepLR",
    "Scheduler_param":{"milestones":[15, 25],
                       "gamma":0.1},
    "Device": 'cuda' if torch.cuda.is_available() else 'cpu'
}

def get_config(config={}):
    merged_config = default_config.copy()
    if config is not None:
        merged_config.update(config)
    return merged_config 

def plot_result(exp_result_dict, save_name='training_results'):
    """
    학습 결과를 시각화하는 함수
    
    Args:
        exp_result_dict: {exp_name: history} 딕셔너리
        save_name: 저장할 파일 이름
    """
    if not exp_result_dict:
        print("No results to plot!")
        return
    
    fig, axes = plt.subplots(1, 2, figsize=(15, 5))
    
    # Loss plot
    for exp_name, history in exp_result_dict.items():
        epochs = range(1, len(history['train_loss']) + 1)
        axes[0].plot(epochs, history['train_loss'], label=f'{exp_name} - Train', linewidth=2)
        axes[0].plot(epochs, history['test_loss'], label=f'{exp_name} - Test', linewidth=2, linestyle='--')
    
    axes[0].set_xlabel('Epoch', fontsize=12)
    axes[0].set_ylabel('Loss', fontsize=12)
    axes[0].set_title('Training and Test Loss', fontsize=14, fontweight='bold')
    axes[0].legend()
    axes[0].grid(True, alpha=0.3)
    
    # Accuracy plot
    for exp_name, history in exp_result_dict.items():
        epochs = range(1, len(history['train_acc']) + 1)
        axes[1].plot(epochs, history['train_acc'], label=f'{exp_name} - Train', linewidth=2)
        axes[1].plot(epochs, history['test_acc'], label=f'{exp_name} - Test', linewidth=2, linestyle='--')
    
    axes[1].set_xlabel('Epoch', fontsize=12)
    axes[1].set_ylabel('Accuracy (%)', fontsize=12)
    axes[1].set_title('Training and Test Accuracy', fontsize=14, fontweight='bold')
    axes[1].legend()
    axes[1].grid(True, alpha=0.3)
    
    plt.tight_layout()
    
    # 결과 저장
    os.makedirs('results', exist_ok=True)
    save_path = os.path.join('results', f'{save_name}.png')
    plt.savefig(save_path, dpi=300, bbox_inches='tight')
    print(f"\n📊 Plot saved to: {save_path}")
    
    plt.show()