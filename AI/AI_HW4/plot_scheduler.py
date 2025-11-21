import torch
import matplotlib.pyplot as plt
from scheduler import scheduler_func_dict

scheduler_dict = {
    "MultiStepLR": {
        "gamma": 0.1,
        "milestones": [15, 25]
    },
    # Add your scheduler here
}

# Initial learning rate
initial_lr = 100
num_epochs = 30

# Plot for each scheduler
plt.figure(figsize=(12, 6))

for scheduler_name, scheduler_params in scheduler_dict.items():
    # Create a fresh model and optimizer for each scheduler
    model = torch.nn.Linear(2, 1)
    optimizer = torch.optim.SGD(model.parameters(), lr=initial_lr)
    
    # Get the lr_lambda function from scheduler_func_dict
    lr_lambda_func = scheduler_func_dict[scheduler_name](scheduler_params)
    
    # Create the scheduler
    scheduler = torch.optim.lr_scheduler.LambdaLR(optimizer, lr_lambda=lr_lambda_func)
    
    # Collect learning rates
    lrs = []
    for epoch in range(num_epochs):
        lrs.append(optimizer.param_groups[0]["lr"])
        optimizer.step()
        scheduler.step()
    
    # Plot
    plt.plot(range(num_epochs), lrs, marker='o', label=scheduler_name)

plt.xlabel('Epoch')
plt.ylabel('Learning Rate')
plt.title('Learning Rate Schedules')
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.savefig('scheduler_comparison.png', dpi=300)
