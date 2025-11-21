from model import LeNet5
from util import get_config, plot_result
from train import train_model

exp_dict = {
    # Example configuration
    # "baseline": {
    #     "Scheduler": "MultiStepLR",
    #     "Learning_rate": 0.1,
    #     "Scheduler_param": {
    #         "gamma": 0.1,
    #         "milestones": [15, 25]
    #     }
    # },
    # "aggressive_decay": {
    #     "Scheduler": "MultiStepLR",
    #     "Learning_rate": 0.1,
    #     "Scheduler_param": {
    #         "gamma": 0.1,
    #         "milestones": [10, 20]
    #     }
    # },
    # "gradual_decay": {
    #     "Scheduler": "MultiStepLR",
    #     "Learning_rate": 0.05,
    #     "Scheduler_param": {
    #         "gamma": 0.5,
    #         "milestones": [30, 45]
    #     }
    # }
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