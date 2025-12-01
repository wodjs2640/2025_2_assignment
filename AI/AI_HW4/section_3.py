from model import LeNet5
from util import get_config, plot_result
from train import train_model

exp_dict = {
    "exp_decay_fast": {
        "Scheduler": "ExponentialDecay",
        "Learning_rate": 0.1,
        "Scheduler_param": {
            "gamma": 0.90
        }
    },
    "exp_decay_moderate": {
        "Scheduler": "ExponentialDecay",
        "Learning_rate": 0.1,
        "Scheduler_param": {
            "gamma": 0.95
        }
    },
    "exp_decay_slow": {
        "Scheduler": "ExponentialDecay",
        "Learning_rate": 0.1,
        "Scheduler_param": {
            "gamma": 0.98
        }
    },
    "cosine_full_decay": {
        "Scheduler": "CosineAnnealingLR",
        "Learning_rate": 0.1,
        "Scheduler_param": {
            "T_max": 50,
            "eta_min_ratio": 0.0
        }
    },
    "cosine_partial_decay": {
        "Scheduler": "CosineAnnealingLR",
        "Learning_rate": 0.1,
        "Scheduler_param": {
            "T_max": 50,
            "eta_min_ratio": 0.1
        }
    },
    "cosine_short_cycle": {
        "Scheduler": "CosineAnnealingLR",
        "Learning_rate": 0.1,
        "Scheduler_param": {
            "T_max": 25,
            "eta_min_ratio": 0.0
        }
    },
    "warmup_cosine_short": {
        "Scheduler": "WarmupCosineDecay",
        "Learning_rate": 0.1,
        "Scheduler_param": {
            "warmup_epochs": 3,
            "T_max": 50,
            "eta_min_ratio": 0.0
        }
    },
    "warmup_cosine_long": {
        "Scheduler": "WarmupCosineDecay",
        "Learning_rate": 0.1,
        "Scheduler_param": {
            "warmup_epochs": 10,
            "T_max": 50,
            "eta_min_ratio": 0.0
        }
    },
    "warmup_cosine_partial": {
        "Scheduler": "WarmupCosineDecay",
        "Learning_rate": 0.1,
        "Scheduler_param": {
            "warmup_epochs": 5,
            "T_max": 50,
            "eta_min_ratio": 0.1
        }
    }
}

def main():
    exp_result_dict = {}
    
    for exp_name, exp_config in exp_dict.items():
        config = get_config(exp_config)
        model = LeNet5()
        
        history = train_model(model, config)
        exp_result_dict[exp_name] = history
    plot_result(exp_result_dict)
    
if __name__ == "__main__":
    main()